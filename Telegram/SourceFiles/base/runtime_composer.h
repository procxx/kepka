//
// This file is part of Kepka,
// an unofficial desktop version of Telegram messaging app,
// see https://github.com/procxx/kepka
//
// Kepka is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// It is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// In addition, as a special exception, the copyright holders give permission
// to link the code of portions of this program with the OpenSSL library.
//
// Full license: https://github.com/procxx/kepka/blob/master/LICENSE
// Copyright (c) 2014-2017 John Preston, https://desktop.telegram.org
// Copyright (c) 2017- Kepka Contributors, https://github.com/procxx
//
#pragma once
#include <QAtomicInt>
#include <cstddef>

#include "base/assertion.h"
#include "core/utils.h"

class RuntimeComposer;
typedef void (*RuntimeComponentConstruct)(void *location, RuntimeComposer *composer);
typedef void (*RuntimeComponentDestruct)(void *location);
typedef void (*RuntimeComponentMove)(void *location, void *waslocation);

struct RuntimeComponentWrapStruct {
	// Don't init any fields, because it is only created in
	// global scope, so it will be filled by zeros from the start.
	RuntimeComponentWrapStruct() = default;
	RuntimeComponentWrapStruct(std::size_t size, std::size_t align, RuntimeComponentConstruct construct,
	                           RuntimeComponentDestruct destruct, RuntimeComponentMove move)
	    : Size(size)
	    , Align(align)
	    , Construct(construct)
	    , Destruct(destruct)
	    , Move(move) {}
	std::size_t Size;
	std::size_t Align;
	RuntimeComponentConstruct Construct;
	RuntimeComponentDestruct Destruct;
	RuntimeComponentMove Move;
};

template <int Value, int Denominator> struct CeilDivideMinimumOne {
	static constexpr int Result = ((Value / Denominator) + ((!Value || (Value % Denominator)) ? 1 : 0));
};

extern RuntimeComponentWrapStruct RuntimeComponentWraps[64];
extern QAtomicInt RuntimeComponentIndexLast;

template <typename Type> struct RuntimeComponent {
	RuntimeComponent() {
		// While there is no std::aligned_alloc().
		static_assert(alignof(Type) <= alignof(std::max_align_t), "Components should align to std::max_align_t!");
	}
	RuntimeComponent(const RuntimeComponent &other) = delete;
	RuntimeComponent &operator=(const RuntimeComponent &other) = delete;
	RuntimeComponent(RuntimeComponent &&other) = delete;
	RuntimeComponent &operator=(RuntimeComponent &&other) = default;

	static int Index() {
		static QAtomicInt MyIndex(0);
		if (auto index = MyIndex.loadAcquire()) {
			return index - 1;
		}
		while (true) {
			auto last = RuntimeComponentIndexLast.loadAcquire();
			if (RuntimeComponentIndexLast.testAndSetOrdered(last, last + 1)) {
				Assert(last < 64);
				if (MyIndex.testAndSetOrdered(0, last + 1)) {
					RuntimeComponentWraps[last] =
					    RuntimeComponentWrapStruct(sizeof(Type), alignof(Type), Type::RuntimeComponentConstruct,
					                               Type::RuntimeComponentDestruct, Type::RuntimeComponentMove);
				}
				break;
			}
		}
		return MyIndex.loadAcquire() - 1;
	}
	static quint64 Bit() {
		return (1ULL << Index());
	}

protected:
	static void RuntimeComponentConstruct(void *location, RuntimeComposer *composer) {
		new (location) Type();
	}
	static void RuntimeComponentDestruct(void *location) {
		((Type *)location)->~Type();
	}
	static void RuntimeComponentMove(void *location, void *waslocation) {
		*(Type *)location = std::move(*(Type *)waslocation);
	}
};

class RuntimeComposerMetadata {
public:
	RuntimeComposerMetadata(quint64 mask)
	    : _mask(mask) {
		for (int i = 0; i != 64; ++i) {
			auto componentBit = (1ULL << i);
			if (_mask & componentBit) {
				auto componentSize = RuntimeComponentWraps[i].Size;
				if (componentSize) {
					auto componentAlign = RuntimeComponentWraps[i].Align;
					if (auto badAlign = (size % componentAlign)) {
						size += (componentAlign - badAlign);
					}
					offsets[i] = size;
					size += componentSize;
					accumulate_max(align, componentAlign);
				}
			} else if (_mask < componentBit) {
				last = i;
				break;
			}
		}
	}

	// Meta pointer in the start.
	std::size_t size = sizeof(const RuntimeComposerMetadata *);
	std::size_t align = alignof(const RuntimeComposerMetadata *);
	std::size_t offsets[64] = {0};
	int last = 64;

	bool equals(quint64 mask) const {
		return _mask == mask;
	}
	quint64 maskadd(quint64 mask) const {
		return _mask | mask;
	}
	quint64 maskremove(quint64 mask) const {
		return _mask & (~mask);
	}

private:
	quint64 _mask;
};

const RuntimeComposerMetadata *GetRuntimeComposerMetadata(quint64 mask);

class RuntimeComposer {
public:
	RuntimeComposer(quint64 mask = 0)
	    : _data(zerodata()) {
		if (mask) {
			auto meta = GetRuntimeComposerMetadata(mask);

			auto data = operator new(meta->size);
			Assert(data != nullptr);

			_data = data;
			_meta() = meta;
			for (int i = 0; i < meta->last; ++i) {
				auto offset = meta->offsets[i];
				if (offset >= sizeof(_meta())) {
					try {
						auto constructAt = _dataptrunsafe(offset);
						auto space = RuntimeComponentWraps[i].Size;
						auto alignedAt = constructAt;
						std::align(RuntimeComponentWraps[i].Align, space, alignedAt, space);
						Assert(alignedAt == constructAt);
						RuntimeComponentWraps[i].Construct(constructAt, this);
					} catch (...) {
						while (i > 0) {
							--i;
							offset = meta->offsets[--i];
							if (offset >= sizeof(_meta())) {
								RuntimeComponentWraps[i].Destruct(_dataptrunsafe(offset));
							}
						}
						throw;
					}
				}
			}
		}
	}
	RuntimeComposer(const RuntimeComposer &other) = delete;
	RuntimeComposer &operator=(const RuntimeComposer &other) = delete;
	~RuntimeComposer() {
		if (_data != zerodata()) {
			auto meta = _meta();
			for (int i = 0; i < meta->last; ++i) {
				auto offset = meta->offsets[i];
				if (offset >= sizeof(_meta())) {
					RuntimeComponentWraps[i].Destruct(_dataptrunsafe(offset));
				}
			}
			operator delete(_data);
		}
	}

	template <typename Type> bool Has() const {
		return (_meta()->offsets[Type::Index()] >= sizeof(_meta()));
	}

	template <typename Type> Type *Get() {
		return static_cast<Type *>(_dataptr(_meta()->offsets[Type::Index()]));
	}
	template <typename Type> const Type *Get() const {
		return static_cast<const Type *>(_dataptr(_meta()->offsets[Type::Index()]));
	}

protected:
	void UpdateComponents(quint64 mask = 0) {
		if (!_meta()->equals(mask)) {
			RuntimeComposer tmp(mask);
			tmp.swap(*this);
			if (_data != zerodata() && tmp._data != zerodata()) {
				auto meta = _meta(), wasmeta = tmp._meta();
				for (int i = 0; i < meta->last; ++i) {
					auto offset = meta->offsets[i];
					auto wasoffset = wasmeta->offsets[i];
					if (offset >= sizeof(_meta()) && wasoffset >= sizeof(_meta())) {
						RuntimeComponentWraps[i].Move(_dataptrunsafe(offset), tmp._dataptrunsafe(wasoffset));
					}
				}
			}
		}
	}
	void AddComponents(quint64 mask = 0) {
		UpdateComponents(_meta()->maskadd(mask));
	}
	void RemoveComponents(quint64 mask = 0) {
		UpdateComponents(_meta()->maskremove(mask));
	}

private:
	static const RuntimeComposerMetadata *ZeroRuntimeComposerMetadata;
	static void *zerodata() {
		return &ZeroRuntimeComposerMetadata;
	}

	void *_dataptrunsafe(size_t skip) const {
		return (char *)_data + skip;
	}
	void *_dataptr(size_t skip) const {
		return (skip >= sizeof(_meta())) ? _dataptrunsafe(skip) : nullptr;
	}
	const RuntimeComposerMetadata *&_meta() const {
		return *static_cast<const RuntimeComposerMetadata **>(_data);
	}
	void *_data = nullptr;

	void swap(RuntimeComposer &other) {
		std::swap(_data, other._data);
	}
};
