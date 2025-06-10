/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 * Code Copy From:
 *      https://github.com/ThisIsClark/Qt-MultiSelectComboBox
 *      License: MIT License
 */
#ifndef MULTI_SELECT_COMBOBOX_H
#define MULTI_SELECT_COMBOBOX_H
/* ================================ [ INCLUDES  ] ============================================== */
#include <QComboBox>
#include <QListWidget>
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
class MultiSelectComboBox : public QComboBox {
  Q_OBJECT

public:
  MultiSelectComboBox(QWidget *aParent = Q_NULLPTR);
  void addItem(const QString &aText, const QVariant &aUserData = QVariant());
  void addItems(const QStringList &aTexts);
  QStringList currentText();
  int count() const;
  void hidePopup() override;
  void SetSearchBarPlaceHolderText(const QString &aPlaceHolderText);
  void SetPlaceHolderText(const QString &aPlaceHolderText);
  void ResetSelection();

signals:
  void selectionChanged();

public slots:
  void clear();
  void setCurrentText(const QString &aText);
  void setCurrentText(const QStringList &aText);

protected:
  void wheelEvent(QWheelEvent *aWheelEvent) override;
  bool eventFilter(QObject *aObject, QEvent *aEvent) override;
  void keyPressEvent(QKeyEvent *aEvent) override;

private:
  void stateChanged(int aState);
  void onSearch(const QString &aSearchString);
  void itemClicked(int aIndex);

  QListWidget *mListWidget;
  QLineEdit *mLineEdit;
  QLineEdit *mSearchBar;
};

/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* MULTI_SELECT_COMBOBOX_H */
