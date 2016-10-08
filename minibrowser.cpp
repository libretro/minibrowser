#include "minibrowser.h"
#include "ui_minibrowser.h"
#include "libretro.h"
#include <stdio.h>
#include <QKeyEvent>
#include <QMouseEvent>

MiniBrowser::MiniBrowser(QWidget *parent) :
  QWidget(parent)
  ,ui(new Ui::MiniBrowser)
  ,m_img(320, 240, QImage::Format_RGB32)
  ,m_format(QImage::Format_RGB32)
{
  ui->setupUi(this);

  connect(ui->urlLineEdit, SIGNAL(returnPressed()), this, SLOT(onURLChanged()));
}

MiniBrowser::~MiniBrowser()
{
  delete ui;
}

void MiniBrowser::onURLChanged() {
  QString text = ui->urlLineEdit->text();

  if(!text.startsWith("http"))
    text.prepend("http://");

  ui->webView->setUrl(text);
}

void MiniBrowser::render() {
  if(!m_img.isNull())
    QWidget::render(&m_img);
}

void MiniBrowser::resizeEvent(QResizeEvent *) {
  m_img = QImage(size(), m_format);
}

void MiniBrowser::setImage(unsigned int width, unsigned int height, QImage::Format format) {
  m_format = format;
  m_img = QImage(QSize(width, height), format);
}

const quint8* MiniBrowser::getImage() {
  return m_img.constBits();
}

void MiniBrowser::onRetroPadInput(int button) {
  switch(button) {
    case RETRO_DEVICE_ID_JOYPAD_SELECT:
      if(ui->urlLineEdit->hasFocus()) {
        ui->webView->setFocus();
      }else{
        ui->urlLineEdit->setFocus();
      }
      break;
    default:
      break;
  }
}

void MiniBrowser::onRetroKeyInput(QtKey key, bool down) {
  if(!down)
    return;

  QWidget *widget = qApp->focusWidget();

  if(widget) {
    QString character = "";

    if(key.character > 0)
      character = key.character;

    QKeyEvent *eventDown = new QKeyEvent(QEvent::KeyPress, key.key, key.modifier, character);
    QKeyEvent *eventUp = new QKeyEvent(QEvent::KeyRelease, key.key, key.modifier, character);

    QApplication::postEvent(widget, eventDown);
    QApplication::postEvent(widget, eventUp);
  }
}

void MiniBrowser::onMouseInput(QtMouse mouse) {
  QWidget *widget = qApp->focusWidget();

  if(widget) {
    if(mouse.newPos != mouse.oldPos) {
      printf("mouse moved to %u x %u\n", mouse.newPos.x(), mouse.newPos.y());
      QMouseEvent *event = new QMouseEvent(QEvent::MouseMove, widget->mapFromGlobal(mouse.newPos), mouse.newPos, Qt::NoButton, Qt::NoButton, Qt::NoModifier);

      QApplication::postEvent(widget, event);
    }

    if(mouse.left) {
      printf("pressed left mouse button\n");
      QMouseEvent *pressEvent = new QMouseEvent(QEvent::MouseButtonPress, widget->mapFromGlobal(mouse.newPos), mouse.newPos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
      QMouseEvent *releaseEvent = new QMouseEvent(QEvent::MouseButtonRelease, widget->mapFromGlobal(mouse.newPos), mouse.newPos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);

      QApplication::postEvent(widget, pressEvent);
      QApplication::postEvent(widget, releaseEvent);
    }

    if(mouse.right) {
      printf("pressed right mouse button\n");
      QMouseEvent *pressEvent = new QMouseEvent(QEvent::MouseButtonPress, widget->mapFromGlobal(mouse.newPos), mouse.newPos, Qt::RightButton, Qt::RightButton, Qt::NoModifier);
      QMouseEvent *releaseEvent = new QMouseEvent(QEvent::MouseButtonRelease, widget->mapFromGlobal(mouse.newPos), mouse.newPos, Qt::RightButton, Qt::RightButton, Qt::NoModifier);

      QApplication::postEvent(widget, pressEvent);
      QApplication::postEvent(widget, releaseEvent);
    }
  }
}
