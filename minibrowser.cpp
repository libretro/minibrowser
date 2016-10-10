#include "minibrowser.h"
#include "ui_minibrowser.h"
#include "libretro.h"
#include <stdio.h>
#include <QKeyEvent>
#include <QMouseEvent>

#define JOYPAD_MOUSE_SPEED 20

MiniBrowser::MiniBrowser(QWidget *parent) :
  QWidget(parent)
  ,ui(new Ui::MiniBrowser)
  ,m_img(320, 240, QImage::Format_RGB32)
  ,m_format(QImage::Format_RGB32)
  ,m_cursor()
  ,m_cursorEnabled(false)
  ,m_mousePos()
  ,m_mouseLeftDown(false)
  ,m_mouseRightDown(false)
  ,m_selectDown(false)
{
  ui->setupUi(this);
  ui->webView->settings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, true);
  ui->webView->settings()->setAttribute(QWebSettings::PluginsEnabled, true);

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

  if(m_cursorEnabled && !m_cursor.isNull()) {
    QPainter p(&m_img);
    p.drawImage(m_mousePos, m_cursor, m_cursor.rect());
  }
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
      if(m_selectDown)
        return;

      m_selectDown = true;

      if(ui->urlLineEdit->hasFocus()) {
        ui->webView->setFocus();
      }else{
        ui->urlLineEdit->setFocus();
      }
      break;
    case RETRO_DEVICE_ID_JOYPAD_A:
      onMouseInput(QtMouse(m_mousePos, m_mousePos, true, false));
      break;
    case RETRO_DEVICE_ID_JOYPAD_UP:
      onMouseInput(QtMouse(m_mousePos, m_mousePos - QPoint(0, JOYPAD_MOUSE_SPEED), false, false));
      break;
    case RETRO_DEVICE_ID_JOYPAD_DOWN:
      onMouseInput(QtMouse(m_mousePos, m_mousePos + QPoint(0, JOYPAD_MOUSE_SPEED), false, false));
      break;
    case RETRO_DEVICE_ID_JOYPAD_LEFT:
      onMouseInput(QtMouse(m_mousePos, m_mousePos - QPoint(JOYPAD_MOUSE_SPEED, 0), false, false));
      break;
    case RETRO_DEVICE_ID_JOYPAD_RIGHT:
      onMouseInput(QtMouse(m_mousePos, m_mousePos + QPoint(JOYPAD_MOUSE_SPEED, 0), false, false));
      break;
    default:
      break;
  }

  m_selectDown = false;
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
      // restrict movement to within the window geometry
      mouse.newPos.setX(qMin(m_img.width(), mouse.newPos.x()));
      mouse.newPos.setX(qMax(0, mouse.newPos.x()));
      mouse.newPos.setY(qMin(m_img.height(), mouse.newPos.y()));
      mouse.newPos.setY(qMax(0, mouse.newPos.y()));

      m_mousePos = mouse.newPos;

      QMouseEvent *event = new QMouseEvent(QEvent::MouseMove, widget->mapFromGlobal(mouse.newPos), mouse.newPos, Qt::NoButton, Qt::NoButton, Qt::NoModifier);

      QApplication::postEvent(widget, event);
    }

    if(mouse.left) {
      if(m_mouseLeftDown)
        return;

      m_mouseLeftDown = true;

      if(!widget->underMouse()) {
        // shift focus to the widget we just clicked on
        QWidget *w = qApp->widgetAt(m_mousePos);

        if(w) {
          w->setFocus();
          widget = w;
        }
      }

      QMouseEvent *pressEvent = new QMouseEvent(QEvent::MouseButtonPress, widget->mapFromGlobal(mouse.newPos), mouse.newPos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
      QMouseEvent *releaseEvent = new QMouseEvent(QEvent::MouseButtonRelease, widget->mapFromGlobal(mouse.newPos), mouse.newPos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);

      QApplication::postEvent(widget, pressEvent);
      QApplication::postEvent(widget, releaseEvent);
    }else{
      m_mouseLeftDown = false;
    }

    if(mouse.right) {
      if(m_mouseRightDown)
        return;

      m_mouseRightDown = true;

      QMouseEvent *pressEvent = new QMouseEvent(QEvent::MouseButtonPress, widget->mapFromGlobal(mouse.newPos), mouse.newPos, Qt::RightButton, Qt::RightButton, Qt::NoModifier);
      QMouseEvent *releaseEvent = new QMouseEvent(QEvent::MouseButtonRelease, widget->mapFromGlobal(mouse.newPos), mouse.newPos, Qt::RightButton, Qt::RightButton, Qt::NoModifier);

      QApplication::postEvent(widget, pressEvent);
      QApplication::postEvent(widget, releaseEvent);
    }else{
      m_mouseRightDown = false;
    }
  }
}

void MiniBrowser::setCursorEnabled(bool on) {
  m_cursorEnabled = on;

  if(m_cursorEnabled) {
    m_cursor = QImage(":/left_ptr.png");
  }
}
