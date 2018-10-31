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

#include "base/flags.h"
#include "chat_helpers/field_autocomplete.h"
#include "core/single_timer.h"
#include "history/history_common.h"
#include "history/history_item.h"
#include "storage/localimageloader.h"
#include "ui/widgets/input_fields.h"
#include "ui/widgets/tooltip.h"
#include "window/section_widget.h"

namespace InlineBots {
namespace Layout {
class ItemBase;
class Widget;
} // namespace Layout
class Result;
} // namespace InlineBots

namespace Ui {
class AbstractButton;
class InnerDropdown;
class DropdownMenu;
class PlainShadow;
class PopupMenu;
class IconButton;
class HistoryDownButton;
class EmojiButton;
class SendButton;
class FlatButton;
class LinkButton;
class RoundButton;
namespace Emoji {
class SuggestionsController;
} // namespace Emoji
} // namespace Ui

namespace Window {
class Controller;
class TopBarWidget;
} // namespace Window

namespace ChatHelpers {
class TabbedPanel;
class TabbedSection;
class TabbedSelector;
} // namespace ChatHelpers

class DragArea;
class SilentToggle;
class SendFilesBox;
class BotKeyboard;
class MessageField;
class HistoryInner;

class ReportSpamPanel : public TWidget {
	Q_OBJECT

public:
	ReportSpamPanel(QWidget *parent);

	void setReported(bool reported, PeerData *onPeer);

signals:
	void hideClicked();
	void reportClicked();
	void clearClicked();

protected:
	void resizeEvent(QResizeEvent *e) override;
	void paintEvent(QPaintEvent *e) override;

private:
	object_ptr<Ui::FlatButton> _report;
	object_ptr<Ui::FlatButton> _hide;
	object_ptr<Ui::LinkButton> _clear;
};

class MainWidget;
class HistoryHider : public TWidget, private base::Subscriber {
	Q_OBJECT

public:
	HistoryHider(MainWidget *parent, const SelectedItemSet &items); // forward messages
	HistoryHider(MainWidget *parent, UserData *sharedContact); // share contact
	HistoryHider(MainWidget *parent); // send path from command line argument
	HistoryHider(MainWidget *parent, const QString &url, const QString &text); // share url
	HistoryHider(MainWidget *parent, const QString &botAndQuery); // inline switch button handler

	bool withConfirm() const;

	bool offerPeer(PeerId peer);
	QString offeredText() const;
	QString botAndQuery() const {
		return _botAndQuery;
	}

	bool wasOffered() const;

	void forwardDone();

	~HistoryHider();

protected:
	void paintEvent(QPaintEvent *e) override;
	void keyPressEvent(QKeyEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;

public slots:
	void startHide();
	void forward();

signals:
	void forwarded();

private:
	void refreshLang();
	void updateControlsGeometry();
	void animationCallback();
	void init();
	MainWidget *parent();

	UserData *_sharedContact = nullptr;
	SelectedItemSet _forwardItems;
	bool _sendPath = false;

	QString _shareUrl, _shareText;
	QString _botAndQuery;

	object_ptr<Ui::RoundButton> _send;
	object_ptr<Ui::RoundButton> _cancel;
	PeerData *_offered = nullptr;

	Animation _a_opacity;

	QRect _box;
	bool _hiding = false;

	mtpRequestId _forwardRequest = 0;

	int _chooseWidth = 0;

	Text _toText;
	qint32 _toTextWidth = 0;
	QPixmap _cacheForAnim;
};

class HistoryWidget final : public Window::AbstractSectionWidget, public RPCSender {
	Q_OBJECT

public:
	HistoryWidget(QWidget *parent, not_null<Window::Controller *> controller);

	void start();

	void messagesReceived(PeerData *peer, const MTPmessages_Messages &messages, mtpRequestId requestId);
	void historyLoaded();

	void windowShown();
	bool doWeReadServerHistory() const;
	bool doWeReadMentions() const;

	void leaveToChildEvent(QEvent *e, QWidget *child) override;
	void dragEnterEvent(QDragEnterEvent *e) override;
	void dragLeaveEvent(QDragLeaveEvent *e) override;
	void dropEvent(QDropEvent *e) override;

	bool isItemCompletelyHidden(HistoryItem *item) const;
	void updateTopBarSelection();

	bool paintTopBar(Painter &p, int decreaseWidth, TimeMs ms);
	QRect getMembersShowAreaGeometry() const;
	void setMembersShowAreaActive(bool active);

	void loadMessages();
	void loadMessagesDown();
	void firstLoadMessages();
	void delayedShowAt(MsgId showAtMsgId);
	void peerMessagesUpdated(PeerId peer);
	void peerMessagesUpdated();

	void newUnreadMsg(History *history, HistoryItem *item);
	void historyToDown(History *history);
	void historyWasRead(ReadServerHistoryChecks checks);
	void unreadCountChanged(History *history);

	QRect historyRect() const;
	int tabbedSelectorSectionWidth() const;
	int minimalWidthForTabbedSelectorSection() const;
	bool willSwitchToTabbedSelectorWithWidth(int newWidth) const;

	void updateSendAction(History *history, SendAction::Type type, qint32 progress = 0);
	void cancelSendAction(History *history, SendAction::Type type);

	void updateRecentStickers();
	void stickersInstalled(quint64 setId);
	void sendActionDone(const MTPBool &result, mtpRequestId req);

	void destroyData();

	void updateFieldPlaceholder();
	void updateStickersByEmoji();

	bool confirmSendingFiles(const QList<QUrl> &files, CompressConfirm compressed = CompressConfirm::Auto,
	                         const QString *addedComment = nullptr);
	bool confirmSendingFiles(const QStringList &files, CompressConfirm compressed = CompressConfirm::Auto,
	                         const QString *addedComment = nullptr);
	bool confirmSendingFiles(const QImage &image, const QByteArray &content,
	                         CompressConfirm compressed = CompressConfirm::Auto,
	                         const QString &insertTextOnCancel = QString());
	bool confirmSendingFiles(const QMimeData *data, CompressConfirm compressed = CompressConfirm::Auto,
	                         const QString &insertTextOnCancel = QString());
	bool confirmShareContact(const QString &phone, const QString &fname, const QString &lname,
	                         const QString *addedComment = nullptr);

	void uploadFile(const QByteArray &fileContent, SendMediaType type);
	void uploadFiles(const QStringList &files, SendMediaType type);

	void sendFileConfirmed(const FileLoadResultPtr &file);

	void updateControlsVisibility();
	void updateControlsGeometry();
	void updateOnlineDisplay();
	void updateOnlineDisplayTimer();

	void onShareContact(const PeerId &peer, UserData *contact);

	void shareContact(const PeerId &peer, const QString &phone, const QString &fname, const QString &lname,
	                  MsgId replyTo, qint32 userId = 0);

	History *history() const;
	PeerData *peer() const;
	void setMsgId(MsgId showAtMsgId);
	MsgId msgId() const;

	bool hasTopBarShadow() const {
		return peer() != nullptr;
	}
	void showAnimated(Window::SlideDirection direction, const Window::SectionSlideParams &params);
	void finishAnimation();

	void doneShow();

	QPoint clampMousePosition(QPoint point);

	void checkSelectingScroll(QPoint point);
	void noSelectingScroll();

	bool touchScroll(const QPoint &delta);

	void enqueueMessageHighlight(not_null<HistoryItem *> item);
	TimeMs highlightStartTime(not_null<const HistoryItem *> item) const;

	SelectedItemSet getSelectedItems() const;
	void itemEdited(HistoryItem *item);

	void updateScrollColors();

	MsgId replyToId() const;
	void messageDataReceived(ChannelData *channel, MsgId msgId);
	bool lastForceReplyReplied(const FullMsgId &replyTo = FullMsgId(NoChannel, -1)) const;
	bool cancelReply(bool lastKeyboardUsed = false);
	void cancelEdit();
	void updateForwarding();
	void updateForwardingTexts();
	void updateForwardingItemRemovedSubscription();

	void clearReplyReturns();
	void pushReplyReturn(HistoryItem *item);
	QList<MsgId> replyReturns();
	void setReplyReturns(PeerId peer, const QList<MsgId> &replyReturns);
	void calcNextReplyReturn();

	void updatePreview();
	void previewCancel();

	void step_recording(double ms, bool timer);
	void stopRecording(bool send);

	void onListEscapePressed();
	void onListEnterPressed();

	void sendBotCommand(PeerData *peer, UserData *bot, const QString &cmd, MsgId replyTo);
	void hideSingleUseKeyboard(PeerData *peer, MsgId replyTo);
	bool insertBotCommand(const QString &cmd);

	bool eventFilter(QObject *obj, QEvent *e) override;

	// With force=true the markup is updated even if it is
	// already shown for the passed history item.
	void updateBotKeyboard(History *h = nullptr, bool force = false);

	DragState getDragState(const QMimeData *d);

	void fastShowAtEnd(History *h);
	void applyDraft(bool parseLinks = true,
	                Ui::FlatTextarea::UndoHistoryAction undoHistoryAction = Ui::FlatTextarea::ClearUndoHistory);
	void showHistory(const PeerId &peer, MsgId showAtMsgId, bool reload = false);
	void clearDelayedShowAt();
	void clearAllLoadRequests();
	void saveFieldToHistoryLocalDraft();

	void applyCloudDraft(History *history);

	void updateHistoryDownPosition();
	void updateHistoryDownVisibility();
	void updateUnreadMentionsPosition();
	void updateUnreadMentionsVisibility();

	void updateFieldSubmitSettings();

	void setInnerFocus();
	bool canSendMessages(PeerData *peer) const;

	void updateNotifySettings();

	void saveGif(DocumentData *doc);

	bool contentOverlapped(const QRect &globalRect);

	void grabStart() override {
		_inGrab = true;
		updateControlsGeometry();
	}
	void grapWithoutTopBarShadow();
	void grabFinish() override;

	bool isItemVisible(HistoryItem *item);

	void confirmDeleteContextItem();
	void confirmDeleteSelectedItems();
	void deleteContextItem(bool forEveryone);
	void deleteSelectedItems(bool forEveryone);

	// Float player interface.
	bool wheelEventFromFloatPlayer(QEvent *e, Window::Column myColumn, Window::Column playerColumn) override;
	QRect rectForFloatPlayer(Window::Column myColumn, Window::Column playerColumn) override;

	void app_sendBotCallback(const HistoryMessageReplyMarkup::Button *button, not_null<const HistoryItem *> msg,
	                         int row, int col);

	void ui_repaintHistoryItem(not_null<const HistoryItem *> item);
	PeerData *ui_getPeerForMouseAction();

	void notify_historyItemLayoutChanged(const HistoryItem *item);
	void notify_botCommandsChanged(UserData *user);
	void notify_inlineBotRequesting(bool requesting);
	void notify_replyMarkupUpdated(const HistoryItem *item);
	void notify_inlineKeyboardMoved(const HistoryItem *item, int oldKeyboardTop, int newKeyboardTop);
	bool notify_switchInlineBotButtonReceived(const QString &query, UserData *samePeerBot, MsgId samePeerReplyTo);
	void notify_userIsBotChanged(UserData *user);
	void notify_migrateUpdated(PeerData *peer);

	bool cmd_search();
	bool cmd_next_chat();
	bool cmd_previous_chat();

	~HistoryWidget();

protected:
	void resizeEvent(QResizeEvent *e) override;
	void keyPressEvent(QKeyEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void paintEvent(QPaintEvent *e) override;
	void leaveEventHook(QEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;

signals:
	void cancelled();
	void historyShown(History *history, MsgId atMsgId);

public slots:
	void onCancel();
	void onReplyToMessage();
	void onEditMessage();
	void onPinMessage();
	void onUnpinMessage();
	void onPinnedHide();
	void onCopyPostLink();
	void onFieldBarCancel();

	void onCancelSendAction();

	void onPreviewParse();
	void onPreviewCheck();
	void onPreviewTimeout();

	void onPhotoUploaded(const FullMsgId &msgId, bool silent, const MTPInputFile &file);
	void onDocumentUploaded(const FullMsgId &msgId, bool silent, const MTPInputFile &file);
	void onThumbDocumentUploaded(const FullMsgId &msgId, bool silent, const MTPInputFile &file,
	                             const MTPInputFile &thumb);

	void onPhotoProgress(const FullMsgId &msgId);
	void onDocumentProgress(const FullMsgId &msgId);

	void onPhotoFailed(const FullMsgId &msgId);
	void onDocumentFailed(const FullMsgId &msgId);

	void onReportSpamClicked();
	void onReportSpamHide();
	void onReportSpamClear();

	void onScroll();
	void onSend(bool ctrlShiftEnter = false, MsgId replyTo = -1);

	void onUnblock();
	void onBotStart();
	void onJoinChannel();
	void onMuteUnmute();
	void onBroadcastSilentChange();

	void onKbToggle(bool manual = true);
	void onCmdStart();

	void activate();
	void onTextChange();

	void onFieldTabbed();
	bool onStickerSend(DocumentData *sticker);
	void onPhotoSend(PhotoData *photo);
	void onInlineResultSend(InlineBots::Result *result, UserData *bot);

	void onWindowVisibleChanged();

	void forwardMessage();
	void selectMessage();

	void onFieldFocused();
	void onFieldResize();
	void onCheckFieldAutocomplete();
	void onScrollTimer();

	void onForwardSelected();
	void onClearSelected();

	void onDraftSaveDelayed();
	void onDraftSave(bool delayed = false);
	void onCloudDraftSave();

	void onRecordError();
	void onRecordDone(QByteArray result, VoiceWaveform waveform, qint32 samples);
	void onRecordUpdate(quint16 level, qint32 samples);

	void onUpdateHistoryItems();

	// checks if we are too close to the top or to the bottom
	// in the scroll area and preloads history if needed
	void preloadHistoryIfNeeded();

private slots:
	void onHashtagOrBotCommandInsert(QString str, FieldAutocomplete::ChooseMethod method);
	void onMentionInsert(UserData *user);
	void onInlineBotCancel();
	void onMembersDropdownShow();

	void onModerateKeyActivate(int index, bool *outHandled);

	void updateField();

private:
	struct SendingFilesLists {
		QList<QUrl> nonLocalUrls;
		QStringList directories;
		QStringList emptyFiles;
		QStringList tooLargeFiles;
		QStringList filesToSend;
		bool allFilesForCompress = true;
	};

	void handlePendingHistoryUpdate();
	void fullPeerUpdated(PeerData *peer);
	void topBarClick();
	void toggleTabbedSelectorMode();
	void updateTabbedSelectorSectionShown();
	void recountChatWidth();
	void setReportSpamStatus(DBIPeerReportSpamStatus status);
	void historyDownClicked();
	void showNextUnreadMention();
	void handlePeerUpdate();

	void highlightMessage(MsgId universalMessageId);
	void adjustHighlightedMessageToMigrated();
	void checkNextHighlight();
	void updateHighlightedMessage();
	void clearHighlightMessages();
	void stopMessageHighlight();

	void animationCallback();
	void updateOverStates(QPoint pos);
	void recordStartCallback();
	void recordStopCallback(bool active);
	void recordUpdateCallback(QPoint globalPos);
	void chooseAttach();
	void historyDownAnimationFinish();
	void unreadMentionsAnimationFinish();
	void sendButtonClicked();
	SendingFilesLists getSendingFilesLists(const QList<QUrl> &files);
	SendingFilesLists getSendingFilesLists(const QStringList &files);
	void getSendingLocalFileInfo(SendingFilesLists &result, const QString &filepath);
	bool confirmSendingFiles(const SendingFilesLists &lists, CompressConfirm compressed = CompressConfirm::Auto,
	                         const QString *addedComment = nullptr);
	template <typename Callback> bool validateSendingFiles(const SendingFilesLists &lists, Callback callback);
	template <typename SendCallback>
	bool showSendFilesBox(object_ptr<SendFilesBox> box, const QString &insertTextOnCancel, const QString *addedComment,
	                      SendCallback callback);

	// If an empty filepath is found we upload (possible) "image" with (possible) "content".
	void uploadFilesAfterConfirmation(const QStringList &files, const QByteArray &content, const QImage &image,
	                                  std::unique_ptr<FileLoadTask::MediaInformation> information, SendMediaType type,
	                                  QString caption);

	void itemRemoved(HistoryItem *item);

	// Updates position of controls around the message field,
	// like send button, emoji button and others.
	void moveFieldControls();
	void updateFieldSize();
	void updateTabbedSelectorToggleTooltipGeometry();
	void checkTabbedSelectorToggleTooltip();

	bool historyHasNotFreezedUnreadBar(History *history) const;
	bool canWriteMessage() const;
	bool isRestrictedWrite() const;
	void orderWidgets();

	void clearInlineBot();
	void inlineBotChanged();

	// Look in the _field for the inline bot and query string.
	void updateInlineBotQuery();

	// Request to show results in the emoji panel.
	void applyInlineBotQuery(UserData *bot, const QString &query);

	void cancelReplyAfterMediaSend(bool lastKeyboardUsed);

	void hideSelectorControlsAnimated();
	int countMembersDropdownHeightMax() const;

	void updateReplyToName();
	void checkForwardingInfo();
	bool editingMessage() const {
		return _editMsgId != 0;
	}

	MsgId _replyToId = 0;
	Text _replyToName;
	int _replyToNameVersion = 0;

	SelectedItemSet _toForward;
	Text _toForwardFrom, _toForwardText;
	int _toForwardNameVersion = 0;
	int _forwardingItemRemovedSubscription = 0;

	int _chatWidth = 0;
	MsgId _editMsgId = 0;

	HistoryItem *_replyEditMsg = nullptr;
	Text _replyEditMsgText;
	mutable SingleTimer _updateEditTimeLeftDisplay;

	object_ptr<Ui::IconButton> _fieldBarCancel;
	void updateReplyEditTexts(bool force = false);

	struct PinnedBar {
		PinnedBar(MsgId msgId, HistoryWidget *parent);
		~PinnedBar();

		MsgId msgId = 0;
		HistoryItem *msg = nullptr;
		Text text;
		object_ptr<Ui::IconButton> cancel;
		object_ptr<Ui::PlainShadow> shadow;
	};
	std::unique_ptr<PinnedBar> _pinnedBar;
	void updatePinnedBar(bool force = false);
	bool pinnedMsgVisibilityUpdated();
	void destroyPinnedBar();
	void unpinDone(const MTPUpdates &updates);

	bool sendExistingDocument(DocumentData *doc, const QString &caption);
	void sendExistingPhoto(PhotoData *photo, const QString &caption);

	void drawField(Painter &p, const QRect &rect);
	void paintEditHeader(Painter &p, const QRect &rect, int left, int top) const;
	void drawRecording(Painter &p, double recordActive);
	void drawPinnedBar(Painter &p);
	void drawRestrictedWrite(Painter &p);

	void updateMouseTracking();

	// destroys _history and _migrated unread bars
	void destroyUnreadBar();

	mtpRequestId _saveEditMsgRequestId = 0;
	void saveEditMsg();
	void saveEditMsgDone(History *history, const MTPUpdates &updates, mtpRequestId req);
	bool saveEditMsgFail(History *history, const RPCError &error, mtpRequestId req);

	static const mtpRequestId ReportSpamRequestNeeded = -1;
	DBIPeerReportSpamStatus _reportSpamStatus = dbiprsUnknown;
	mtpRequestId _reportSpamSettingRequestId = ReportSpamRequestNeeded;
	void updateReportSpamStatus();
	void requestReportSpamSetting();
	void reportSpamSettingDone(const MTPPeerSettings &result, mtpRequestId req);
	bool reportSpamSettingFail(const RPCError &error, mtpRequestId req);

	QString _previewLinks;
	WebPageData *_previewData = nullptr;
	typedef QMap<QString, WebPageId> PreviewCache;
	PreviewCache _previewCache;
	mtpRequestId _previewRequest = 0;
	Text _previewTitle;
	Text _previewDescription;
	SingleTimer _previewTimer;
	bool _previewCancelled = false;
	void gotPreview(QString links, const MTPMessageMedia &media, mtpRequestId req);

	bool _replyForwardPressed = false;

	HistoryItem *_replyReturn = nullptr;
	QList<MsgId> _replyReturns;

	bool messagesFailed(const RPCError &error, mtpRequestId requestId);
	void addMessagesToFront(PeerData *peer, const QVector<MTPMessage> &messages);
	void addMessagesToBack(PeerData *peer, const QVector<MTPMessage> &messages);

	struct BotCallbackInfo {
		UserData *bot;
		FullMsgId msgId;
		int row, col;
		bool game;
	};
	void botCallbackDone(BotCallbackInfo info, const MTPmessages_BotCallbackAnswer &answer, mtpRequestId req);
	bool botCallbackFail(BotCallbackInfo info, const RPCError &error, mtpRequestId req);

	enum ScrollChangeType {
		ScrollChangeNone,

		// When we toggle a pinned message.
		ScrollChangeAdd,

		// When loading a history part while scrolling down.
		ScrollChangeNoJumpToBottom,
	};
	struct ScrollChange {
		ScrollChangeType type;
		int value;
	};
	void updateHistoryGeometry(bool initial = false, bool loadedDown = false,
	                           const ScrollChange &change = {ScrollChangeNone, 0});
	void updateListSize();

	// Does any of the shown histories has this flag set.
	bool hasPendingResizedItems() const {
		return (_history && _history->hasPendingResizedItems()) || (_migrated && _migrated->hasPendingResizedItems());
	}

	// Counts scrollTop for placing the scroll right at the unread
	// messages bar, choosing from _history and _migrated unreadBar.
	int unreadBarTop() const;
	int itemTopForHighlight(not_null<HistoryItem *> item) const;
	void scrollToCurrentVoiceMessage(FullMsgId fromId, FullMsgId toId);

	// Scroll to current y without updating the _lastUserScrolled time.
	// Used to distinguish between user scrolls and syntetic scrolls.
	// This one is syntetic.
	void synteticScrollToY(int y);

	void saveGifDone(DocumentData *doc, const MTPBool &result);

	void reportSpamDone(PeerData *peer, const MTPBool &result, mtpRequestId request);
	bool reportSpamFail(const RPCError &error, mtpRequestId request);

	void unblockDone(PeerData *peer, const MTPBool &result, mtpRequestId req);
	bool unblockFail(const RPCError &error, mtpRequestId req);
	void blockDone(PeerData *peer, const MTPBool &result);

	void joinDone(const MTPUpdates &result, mtpRequestId req);
	bool joinFail(const RPCError &error, mtpRequestId req);

	void countHistoryShowFrom();

	enum class TextUpdateEvent {
		SaveDraft = (1 << 0),
		SendTyping = (1 << 1),
	};
	using TextUpdateEvents = base::flags<TextUpdateEvent>;
	friend inline constexpr bool is_flag_type(TextUpdateEvent) {
		return true;
	};

	void writeDrafts(Data::Draft **localDraft, Data::Draft **editDraft);
	void writeDrafts(History *history);
	void setFieldText(const TextWithTags &textWithTags, TextUpdateEvents events = 0,
	                  Ui::FlatTextarea::UndoHistoryAction undoHistoryAction = Ui::FlatTextarea::ClearUndoHistory);
	void clearFieldText(TextUpdateEvents events = 0,
	                    Ui::FlatTextarea::UndoHistoryAction undoHistoryAction = Ui::FlatTextarea::ClearUndoHistory) {
		setFieldText(TextWithTags(), events, undoHistoryAction);
	}

	HistoryItem *getItemFromHistoryOrMigrated(MsgId genericMsgId) const;
	void animatedScrollToItem(MsgId msgId);
	void animatedScrollToY(int scrollTo, HistoryItem *attachTo = nullptr);

	void updateDragAreas();

	// when scroll position or scroll area size changed this method
	// updates the boundings of the visible area in HistoryInner
	void visibleAreaUpdated();
	int countInitialScrollTop();
	int countAutomaticScrollTop();
	void preloadHistoryByScroll();
	void checkReplyReturns();
	void scrollToAnimationCallback(FullMsgId attachToId);

	bool readyToForward() const;
	bool hasSilentToggle() const;

	PeerData *_peer = nullptr;

	ChannelId _channel = NoChannel;
	bool _canSendMessages = false;
	MsgId _showAtMsgId = ShowAtUnreadMsgId;

	mtpRequestId _firstLoadRequest = 0;
	mtpRequestId _preloadRequest = 0;
	mtpRequestId _preloadDownRequest = 0;

	MsgId _delayedShowAtMsgId = -1; // wtf?
	mtpRequestId _delayedShowAtRequest = 0;

	object_ptr<Ui::AbstractButton> _backAnimationButton = {nullptr};
	object_ptr<Window::TopBarWidget> _topBar;
	object_ptr<Ui::ScrollArea> _scroll;
	QPointer<HistoryInner> _list;
	History *_migrated = nullptr;
	History *_history = nullptr;
	bool _historyInited = false; // Initial updateHistoryGeometry() was called.
	bool _updateHistoryGeometryRequired = false; // If updateListSize() was called without updateHistoryGeometry().
	int _addToScroll = 0;

	int _lastScrollTop = 0; // gifs optimization
	TimeMs _lastScrolled = 0;
	QTimer _updateHistoryItems;

	TimeMs _lastUserScrolled = 0;
	bool _synteticScrollEvent = false;
	Animation _scrollToAnimation;

	Animation _historyDownShown;
	bool _historyDownIsShown = false;
	object_ptr<Ui::HistoryDownButton> _historyDown;

	Animation _unreadMentionsShown;
	bool _unreadMentionsIsShown = false;
	object_ptr<Ui::HistoryDownButton> _unreadMentions;

	object_ptr<FieldAutocomplete> _fieldAutocomplete;

	UserData *_inlineBot = nullptr;
	QString _inlineBotUsername;
	mtpRequestId _inlineBotResolveRequestId = 0;
	bool _isInlineBot = false;
	void inlineBotResolveDone(const MTPcontacts_ResolvedPeer &result);
	bool inlineBotResolveFail(QString name, const RPCError &error);

	bool isBotStart() const;
	bool isBlocked() const;
	bool isJoinChannel() const;
	bool isMuteUnmute() const;
	bool updateCmdStartShown();
	void updateSendButtonType();
	bool showRecordButton() const;
	bool showInlineBotCancel() const;

	object_ptr<ReportSpamPanel> _reportSpamPanel = {nullptr};

	object_ptr<Ui::SendButton> _send;
	object_ptr<Ui::FlatButton> _unblock;
	object_ptr<Ui::FlatButton> _botStart;
	object_ptr<Ui::FlatButton> _joinChannel;
	object_ptr<Ui::FlatButton> _muteUnmute;
	mtpRequestId _unblockRequest = 0;
	mtpRequestId _reportSpamRequest = 0;
	object_ptr<Ui::IconButton> _attachToggle;
	object_ptr<Ui::EmojiButton> _tabbedSelectorToggle;
	object_ptr<Ui::ImportantTooltip> _tabbedSelectorToggleTooltip = {nullptr};
	bool _tabbedSelectorToggleTooltipShown = false;
	object_ptr<Ui::IconButton> _botKeyboardShow;
	object_ptr<Ui::IconButton> _botKeyboardHide;
	object_ptr<Ui::IconButton> _botCommandStart;
	object_ptr<SilentToggle> _silent;
	bool _cmdStartShown = false;
	object_ptr<MessageField> _field;
	bool _recording = false;
	bool _inField = false;
	bool _inReplyEditForward = false;
	bool _inPinnedMsg = false;
	bool _inClickable = false;
	int _recordingSamples = 0;
	int _recordCancelWidth;

	// This can animate for a very long time (like in music playing),
	// so it should be a BasicAnimation, not an Animation.
	BasicAnimation _a_recording;
	anim::value a_recordingLevel;

	bool kbWasHidden() const;

	bool _kbShown = false;
	HistoryItem *_kbReplyTo = nullptr;
	object_ptr<Ui::ScrollArea> _kbScroll;
	QPointer<BotKeyboard> _keyboard;

	object_ptr<Ui::InnerDropdown> _membersDropdown = {nullptr};
	QTimer _membersDropdownShowTimer;

	object_ptr<InlineBots::Layout::Widget> _inlineResults = {nullptr};
	object_ptr<ChatHelpers::TabbedPanel> _tabbedPanel;
	object_ptr<ChatHelpers::TabbedSection> _tabbedSection = {nullptr};
	QPointer<ChatHelpers::TabbedSelector> _tabbedSelector;
	bool _tabbedSectionUsed = false;
	DragState _attachDrag = DragStateNone;
	object_ptr<DragArea> _attachDragDocument, _attachDragPhoto;

	object_ptr<Ui::Emoji::SuggestionsController> _emojiSuggestions = {nullptr};

	bool _nonEmptySelection = false;

	TaskQueue _fileLoader;
	TextUpdateEvents _textUpdateEvents =
	    (TextUpdateEvents() | TextUpdateEvent::SaveDraft | TextUpdateEvent::SendTyping);

	qint64 _serviceImageCacheSize = 0;
	QString _confirmSource;

	QString _titlePeerText;
	bool _titlePeerTextOnline = false;
	int _titlePeerTextWidth = 0;

	Animation _a_show;
	Window::SlideDirection _showDirection;
	QPixmap _cacheUnder, _cacheOver;

	QTimer _scrollTimer;
	qint32 _scrollDelta = 0;

	MsgId _highlightedMessageId = 0;
	std::deque<MsgId> _highlightQueue;
	base::Timer _highlightTimer;
	TimeMs _highlightStart = 0;

	QMap<QPair<History *, SendAction::Type>, mtpRequestId> _sendActionRequests;
	QTimer _sendActionStopTimer;

	TimeMs _saveDraftStart = 0;
	bool _saveDraftText = false;
	QTimer _saveDraftTimer, _saveCloudDraftTimer;

	object_ptr<Ui::PlainShadow> _topShadow;
	object_ptr<Ui::PlainShadow> _rightShadow = {nullptr};
	bool _inGrab = false;
};
