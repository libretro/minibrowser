#ifndef MINIBROWSER_H
#define MINIBROWSER_H

#include <QWidget>

namespace Ui {
  class MiniBrowser;
}

struct QtKey {
   QtKey(Qt::Key key, quint32 character = 0, Qt::KeyboardModifier modifier = Qt::NoModifier) :
   key(key)
   ,character(character)
   ,modifier(modifier)
   {}

   Qt::Key key;
   quint32 character;
   Qt::KeyboardModifier modifier;
};

struct QtMouse {
  QtMouse(QPoint oldPos, QPoint newPos, bool left, bool right) :
  oldPos(oldPos)
  ,newPos(newPos)
  ,left(left)
  ,right(right)
  {}

  QPoint oldPos;
  QPoint newPos;
  bool left;
  bool right;
};

class MiniBrowser : public QWidget
{
  Q_OBJECT

public:
  explicit MiniBrowser(QWidget *parent = 0);
  ~MiniBrowser();
  void render();
  void setImage(unsigned int width, unsigned int height, QImage::Format format);
  const quint8* getImage();
  void onRetroPadInput(int button);
  void onRetroKeyInput(QtKey key, bool down);
  void onMouseInput(QtMouse mouse);

private slots:
  void onURLChanged();

protected:
  void resizeEvent(QResizeEvent *event);

private:
  Ui::MiniBrowser *ui;
  QImage m_img;
  QImage::Format m_format;
};

#endif // MINIBROWSER_H
