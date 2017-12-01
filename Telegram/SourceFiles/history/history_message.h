/*
This file is part of Telegram Desktop,
the official desktop version of Telegram messaging app, see https://telegram.org

Telegram Desktop is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

It is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

In addition, as a special exception, the copyright holders give permission
to link the code of portions of this program with the OpenSSL library.

Full license: https://github.com/telegramdesktop/tdesktop/blob/master/LICENSE
Copyright (c) 2014-2017 John Preston, https://desktop.telegram.org
*/
#pragma once

void HistoryInitMessages();
base::lambda<void(ChannelData*, MsgId)> HistoryDependentItemCallback(const FullMsgId &msgId);
MTPDmessage::Flags NewMessageFlags(not_null<PeerData*> peer);
QString GetErrorTextForForward(not_null<PeerData*> peer, const SelectedItemSet &items);
void FastShareMessage(not_null<HistoryItem*> item);

class HistoryMessage : public HistoryItem, private HistoryItemInstantiated<HistoryMessage> {
public:
	static not_null<HistoryMessage*> create(not_null<History*> history, const MTPDmessage &msg) {
		return _create(history, msg);
	}
	static not_null<HistoryMessage*> create(not_null<History*> history, const MTPDmessageService &msg) {
		return _create(history, msg);
	}
	static not_null<HistoryMessage*> create(not_null<History*> history, MsgId msgId, MTPDmessage::Flags flags, QDateTime date, UserId from, const QString &postAuthor, not_null<HistoryMessage*> fwd) {
		return _create(history, msgId, flags, date, from, postAuthor, fwd);
	}
	static not_null<HistoryMessage*> create(not_null<History*> history, MsgId msgId, MTPDmessage::Flags flags, MsgId replyTo, UserId viaBotId, QDateTime date, UserId from, const QString &postAuthor, const TextWithEntities &textWithEntities) {
		return _create(history, msgId, flags, replyTo, viaBotId, date, from, postAuthor, textWithEntities);
	}
	static not_null<HistoryMessage*> create(not_null<History*> history, MsgId msgId, MTPDmessage::Flags flags, MsgId replyTo, UserId viaBotId, QDateTime date, UserId from, const QString &postAuthor, DocumentData *doc, const QString &caption, const MTPReplyMarkup &markup) {
		return _create(history, msgId, flags, replyTo, viaBotId, date, from, postAuthor, doc, caption, markup);
	}
	static not_null<HistoryMessage*> create(not_null<History*> history, MsgId msgId, MTPDmessage::Flags flags, MsgId replyTo, UserId viaBotId, QDateTime date, UserId from, const QString &postAuthor, PhotoData *photo, const QString &caption, const MTPReplyMarkup &markup) {
		return _create(history, msgId, flags, replyTo, viaBotId, date, from, postAuthor, photo, caption, markup);
	}
	static not_null<HistoryMessage*> create(not_null<History*> history, MsgId msgId, MTPDmessage::Flags flags, MsgId replyTo, UserId viaBotId, QDateTime date, UserId from, const QString &postAuthor, GameData *game, const MTPReplyMarkup &markup) {
		return _create(history, msgId, flags, replyTo, viaBotId, date, from, postAuthor, game, markup);
	}

	void initTime();
	void initMedia(const MTPMessageMedia *media);
	void initMediaFromDocument(DocumentData *doc, const QString &caption);
	void fromNameUpdated(int32_t width) const;

	int32_t plainMaxWidth() const;
	QRect countGeometry() const;

	bool drawBubble() const;
	bool hasBubble() const override {
		return drawBubble();
	}
	bool displayFromName() const {
		if (!hasFromName()) return false;
		if (isAttachedToPrevious()) return false;
		return true;
	}
	bool displayEditedBadge(bool hasViaBotOrInlineMarkup) const;
	bool uploading() const;
	bool displayFastShare() const override;

	void drawInfo(Painter &p, int32_t right, int32_t bottom, int32_t width, bool selected, InfoDisplayType type) const override;
	void drawFastShare(Painter &p, int left, int top, int outerWidth) const override;
	void setViewsCount(int32_t count) override;
	void setId(MsgId newId) override;
	void draw(Painter &p, QRect clip, TextSelection selection, TimeMs ms) const override;
	ClickHandlerPtr fastShareLink() const override;

	void dependencyItemRemoved(HistoryItem *dependency) override;

	bool hasPoint(QPoint point) const override;
	bool pointInTime(int right, int bottom, QPoint point, InfoDisplayType type) const override;

	HistoryTextState getState(QPoint point, HistoryStateRequest request) const override;
	void updatePressed(QPoint point) override;

	TextSelection adjustSelection(TextSelection selection, TextSelectType type) const override;

	// ClickHandlerHost interface
	void clickHandlerActiveChanged(const ClickHandlerPtr &p, bool active) override;
	void clickHandlerPressedChanged(const ClickHandlerPtr &p, bool pressed) override;

	QString notificationHeader() const override;

	void applyEdition(const MTPDmessage &message) override;
	void applyEdition(const MTPDmessageService &message) override;
	void updateMedia(const MTPMessageMedia *media) override;
	void updateReplyMarkup(const MTPReplyMarkup *markup) override {
		setReplyMarkup(markup);
	}
	int32_t addToOverview(AddToOverviewMethod method) override;
	void eraseFromOverview() override;

	TextWithEntities selectedText(TextSelection selection) const override;
	void setText(const TextWithEntities &textWithEntities) override;
	TextWithEntities originalText() const override;
	bool textHasLinks() const override;

	int infoWidth() const override;
	int timeLeft() const override;
	int timeWidth() const override {
		return _timeWidth;
	}

	int viewsCount() const override {
		if (auto views = Get<HistoryMessageViews>()) {
			return views->_views;
		}
		return HistoryItem::viewsCount();
	}

	bool updateDependencyItem() override {
		if (auto reply = Get<HistoryMessageReply>()) {
			return reply->updateData(this, true);
		}
		return true;
	}
	MsgId dependencyMsgId() const override {
		return replyToId();
	}

	HistoryMessage *toHistoryMessage() override { // dynamic_cast optimize
		return this;
	}
	const HistoryMessage *toHistoryMessage() const override { // dynamic_cast optimize
		return this;
	}

	// hasFromPhoto() returns true even if we don't display the photo
	// but we need to skip a place at the left side for this photo
	bool displayFromPhoto() const;
	bool hasFromPhoto() const;

	~HistoryMessage();

private:
	HistoryMessage(not_null<History*> history, const MTPDmessage &msg);
	HistoryMessage(not_null<History*> history, const MTPDmessageService &msg);
	HistoryMessage(not_null<History*> history, MsgId msgId, MTPDmessage::Flags flags, QDateTime date, UserId from, const QString &postAuthor, not_null<HistoryMessage*> fwd); // local forwarded
	HistoryMessage(not_null<History*> history, MsgId msgId, MTPDmessage::Flags flags, MsgId replyTo, UserId viaBotId, QDateTime date, UserId from, const QString &postAuthor, const TextWithEntities &textWithEntities); // local message
	HistoryMessage(not_null<History*> history, MsgId msgId, MTPDmessage::Flags flags, MsgId replyTo, UserId viaBotId, QDateTime date, UserId from, const QString &postAuthor, DocumentData *doc, const QString &caption, const MTPReplyMarkup &markup); // local document
	HistoryMessage(not_null<History*> history, MsgId msgId, MTPDmessage::Flags flags, MsgId replyTo, UserId viaBotId, QDateTime date, UserId from, const QString &postAuthor, PhotoData *photo, const QString &caption, const MTPReplyMarkup &markup); // local photo
	HistoryMessage(not_null<History*> history, MsgId msgId, MTPDmessage::Flags flags, MsgId replyTo, UserId viaBotId, QDateTime date, UserId from, const QString &postAuthor, GameData *game, const MTPReplyMarkup &markup); // local game
	friend class HistoryItemInstantiated<HistoryMessage>;

	void setEmptyText();

	// For an invoice button we replace the button text with a "Receipt" key.
	// It should show the receipt for the payed invoice. Still let mobile apps do that.
	void replaceBuyWithReceiptInMarkup();

	void initDimensions() override;
	int resizeContentGetHeight() override;
	int performResizeGetHeight();
	void applyEditionToEmpty();

	bool displayForwardedFrom() const;
	void paintFromName(Painter &p, QRect &trect, bool selected) const;
	void paintForwardedInfo(Painter &p, QRect &trect, bool selected) const;
	void paintReplyInfo(Painter &p, QRect &trect, bool selected) const;
	// this method draws "via @bot" if it is not painted in forwarded info or in from name
	void paintViaBotIdInfo(Painter &p, QRect &trect, bool selected) const;
	void paintText(Painter &p, QRect &trect, TextSelection selection) const;

	bool getStateFromName(QPoint point, QRect &trect, HistoryTextState *outResult) const;
	bool getStateForwardedInfo(QPoint point, QRect &trect, HistoryTextState *outResult, const HistoryStateRequest &request) const;
	bool getStateReplyInfo(QPoint point, QRect &trect, HistoryTextState *outResult) const;
	bool getStateViaBotIdInfo(QPoint point, QRect &trect, HistoryTextState *outResult) const;
	bool getStateText(QPoint point, QRect &trect, HistoryTextState *outResult, const HistoryStateRequest &request) const;

	void setMedia(const MTPMessageMedia *media);
	void setReplyMarkup(const MTPReplyMarkup *markup);

	QString _timeText;
	int _timeWidth = 0;

	mutable ClickHandlerPtr _fastShareLink;

	struct CreateConfig {
		MsgId replyTo = 0;
		UserId viaBotId = 0;
		int viewsCount = -1;
		QString author;
		PeerId senderOriginal = 0;
		MsgId originalId = 0;
		QString authorOriginal;
		QDateTime originalDate;
		QDateTime editDate;

		// For messages created from MTP structs.
		const MTPReplyMarkup *mtpMarkup = nullptr;

		// For messages created from existing messages (forwarded).
		const HistoryMessageReplyMarkup *inlineMarkup = nullptr;
	};
	void createComponentsHelper(MTPDmessage::Flags flags, MsgId replyTo, UserId viaBotId, const QString &postAuthor, const MTPReplyMarkup &markup);
	void createComponents(const CreateConfig &config);

	class KeyboardStyle : public ReplyKeyboard::Style {
	public:
		using ReplyKeyboard::Style::Style;

		int buttonRadius() const override;

		void startPaint(Painter &p) const override;
		const style::TextStyle &textStyle() const override;
		void repaint(not_null<const HistoryItem*> item) const override;

	protected:
		void paintButtonBg(Painter &p, const QRect &rect, double howMuchOver) const override;
		void paintButtonIcon(Painter &p, const QRect &rect, int outerWidth, HistoryMessageReplyMarkup::Button::Type type) const override;
		void paintButtonLoading(Painter &p, const QRect &rect) const override;
		int minButtonWidth(HistoryMessageReplyMarkup::Button::Type type) const override;

	};

	void updateMediaInBubbleState();

};
