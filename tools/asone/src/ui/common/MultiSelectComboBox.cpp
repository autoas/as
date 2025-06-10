/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "MultiSelectComboBox.hpp"
#include <QLineEdit>
#include <QCheckBox>
#include <QEvent>
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static const int scSearchBarIndex = 0;
/* ================================ [ FUNCTIONS ] ============================================== */
MultiSelectComboBox::MultiSelectComboBox(QWidget *aParent)
  : QComboBox(aParent),
    mListWidget(new QListWidget(this)),
    mLineEdit(new QLineEdit(this)),
    mSearchBar(new QLineEdit(this)) {
  QListWidgetItem *curItem = new QListWidgetItem(mListWidget);
  mSearchBar->setPlaceholderText("Search..");
  mSearchBar->setClearButtonEnabled(true);
  mListWidget->addItem(curItem);
  mListWidget->setItemWidget(curItem, mSearchBar);

  mLineEdit->setReadOnly(true);
  mLineEdit->installEventFilter(this);

  setModel(mListWidget->model());
  setView(mListWidget);
  setLineEdit(mLineEdit);

  connect(mSearchBar, &QLineEdit::textChanged, this, &MultiSelectComboBox::onSearch);
  connect(this, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this,
          &MultiSelectComboBox::itemClicked);
}

void MultiSelectComboBox::hidePopup() {
  int width = this->width();
  int height = mListWidget->height();
  int x = QCursor::pos().x() - mapToGlobal(geometry().topLeft()).x() + geometry().x();
  int y = QCursor::pos().y() - mapToGlobal(geometry().topLeft()).y() + geometry().y();
  if (x >= 0 && x <= width && y >= this->height() && y <= height + this->height()) {
    // Item was clicked, do not hide popup
  } else {
    QComboBox::hidePopup();
  }
}

void MultiSelectComboBox::stateChanged(int aState) {
  Q_UNUSED(aState);
  QString selectedData("");
  int count = mListWidget->count();

  for (int i = 1; i < count; ++i) {
    QWidget *widget = mListWidget->itemWidget(mListWidget->item(i));
    QCheckBox *checkBox = static_cast<QCheckBox *>(widget);

    if (checkBox->isChecked()) {
      selectedData.append(checkBox->text()).append(";");
    }
  }
  if (selectedData.endsWith(";")) {
    selectedData.remove(selectedData.size() - 1, 1);
  }
  if (!selectedData.isEmpty()) {
    mLineEdit->setText(selectedData);
  } else {
    mLineEdit->clear();
  }

  mLineEdit->setToolTip(selectedData);
  emit selectionChanged();
}

void MultiSelectComboBox::addItem(const QString &aText, const QVariant &aUserData) {
  Q_UNUSED(aUserData);
  QListWidgetItem *listWidgetItem = new QListWidgetItem(mListWidget);
  QCheckBox *checkBox = new QCheckBox(this);
  checkBox->setText(aText);
  mListWidget->addItem(listWidgetItem);
  mListWidget->setItemWidget(listWidgetItem, checkBox);
  connect(checkBox, &QCheckBox::stateChanged, this, &MultiSelectComboBox::stateChanged);
}

QStringList MultiSelectComboBox::currentText() {
  QStringList emptyStringList;
  if (!mLineEdit->text().isEmpty()) {
    emptyStringList = mLineEdit->text().split(';');
  }
  return emptyStringList;
}

void MultiSelectComboBox::addItems(const QStringList &aTexts) {
  for (const auto &string : aTexts) {
    addItem(string);
  }
}

int MultiSelectComboBox::count() const {
  int count = mListWidget->count() - 1; // Do not count the search bar
  if (count < 0) {
    count = 0;
  }
  return count;
}

void MultiSelectComboBox::onSearch(const QString &aSearchString) {
  for (int i = 1; i < mListWidget->count(); i++) {
    QCheckBox *checkBox = static_cast<QCheckBox *>(mListWidget->itemWidget(mListWidget->item(i)));
    if (checkBox->text().contains(aSearchString, Qt::CaseInsensitive)) {
      mListWidget->item(i)->setHidden(false);
    } else {
      mListWidget->item(i)->setHidden(true);
    }
  }
}

void MultiSelectComboBox::itemClicked(int aIndex) {
  if (aIndex != scSearchBarIndex) // 0 means the search bar
  {
    QWidget *widget = mListWidget->itemWidget(mListWidget->item(aIndex));
    QCheckBox *checkBox = static_cast<QCheckBox *>(widget);
    checkBox->setChecked(!checkBox->isChecked());
  }
}

void MultiSelectComboBox::SetSearchBarPlaceHolderText(const QString &aPlaceHolderText) {
  mSearchBar->setPlaceholderText(aPlaceHolderText);
}

void MultiSelectComboBox::SetPlaceHolderText(const QString &aPlaceHolderText) {
  mLineEdit->setPlaceholderText(aPlaceHolderText);
}

void MultiSelectComboBox::clear() {
  mListWidget->clear();
  QListWidgetItem *curItem = new QListWidgetItem(mListWidget);
  mSearchBar = new QLineEdit(this);
  mSearchBar->setPlaceholderText("Search..");
  mSearchBar->setClearButtonEnabled(true);
  mListWidget->addItem(curItem);
  mListWidget->setItemWidget(curItem, mSearchBar);

  connect(mSearchBar, &QLineEdit::textChanged, this, &MultiSelectComboBox::onSearch);
}

void MultiSelectComboBox::wheelEvent(QWheelEvent *aWheelEvent) {
  // Do not handle the wheel event
  Q_UNUSED(aWheelEvent);
}

bool MultiSelectComboBox::eventFilter(QObject *aObject, QEvent *aEvent) {
  if (aObject == mLineEdit && aEvent->type() == QEvent::MouseButtonRelease) {
    showPopup();
    return false;
  }
  return false;
}

void MultiSelectComboBox::keyPressEvent(QKeyEvent *aEvent) {
  // Do not handle key event
  Q_UNUSED(aEvent);
}

void MultiSelectComboBox::setCurrentText(const QString &aText) {
  Q_UNUSED(aText);
}

void MultiSelectComboBox::setCurrentText(const QStringList &aText) {
  int count = mListWidget->count();

  for (int i = 1; i < count; ++i) {
    QWidget *widget = mListWidget->itemWidget(mListWidget->item(i));
    QCheckBox *checkBox = static_cast<QCheckBox *>(widget);
    QString checkBoxString = checkBox->text();
    if (aText.contains(checkBoxString)) {
      checkBox->setChecked(true);
    }
  }
}

void MultiSelectComboBox::ResetSelection() {
  int count = mListWidget->count();

  for (int i = 1; i < count; ++i) {
    QWidget *widget = mListWidget->itemWidget(mListWidget->item(i));
    QCheckBox *checkBox = static_cast<QCheckBox *>(widget);
    checkBox->setChecked(false);
  }
}
