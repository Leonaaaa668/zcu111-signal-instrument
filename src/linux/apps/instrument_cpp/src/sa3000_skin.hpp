#pragma once
#include <QtWidgets/QWidget>
#include <QtWidgets/QPushButton>
#include <QtGui/QPainter>
#include <QtGui/QLinearGradient>
#include <QtGui/QResizeEvent>
#include <QtGui/QWheelEvent>
#include <functional>
#include <cmath>

// Native child widgets, not a GraphicsProxyWidget: only changed plots repaint.
// Coordinates and colors follow the user's SA3000 PyQt5 design canvas.
class SaCanvas final:public QWidget {
public:
    std::function<void()> resized;
    std::function<void(int)> dial;
    explicit SaCanvas(QWidget *parent=nullptr):QWidget(parent){setMinimumSize(960,540);}
    double scale()const{return std::min(width()/1600.,height()/900.);}
    QPointF origin()const{const auto s=scale();return {(width()-1600*s)/2,(height()-900*s)/2};}
    QRect map(const QRectF &r)const{const auto s=scale();const auto o=origin();return QRectF(o.x()+r.x()*s,o.y()+r.y()*s,r.width()*s,r.height()*s).toAlignedRect();}
protected:
    void resizeEvent(QResizeEvent *event)override{QWidget::resizeEvent(event);if(resized)resized();}
    void wheelEvent(QWheelEvent *event)override{
#if QT_VERSION >= QT_VERSION_CHECK(5,14,0)
        const auto pos=event->position();
#else
        const auto pos=event->posF();
#endif
        if(map({1374,407,156,156}).contains(pos.toPoint())&&dial){dial(event->angleDelta().y()>0?1:-1);event->accept();}
        else QWidget::wheelEvent(event);
    }
    void paintEvent(QPaintEvent*)override{
        QPainter p(this);p.fillRect(rect(),QColor("#0c1319"));p.translate(origin());p.scale(scale(),scale());
        p.setRenderHint(QPainter::Antialiasing);p.fillRect(QRectF(0,0,1600,900),QColor("#10191f"));
        auto box=[&](double x,double y,double w,double h,const char *fill,const char *edge,double r){p.setPen(QPen(QColor(edge),1));p.setBrush(QColor(fill));p.drawRoundedRect(QRectF(x,y,w,h),r,r);};
        auto line=[&](double x,double y,double xx,double yy,const char *color){p.setPen(QPen(QColor(color),1));p.drawLine(QPointF(x,y),QPointF(xx,yy));};
        auto label=[&](double x,double y,const QString &s,int size,const char *color,bool bold=false){QFont f("DejaVu Sans");f.setPixelSize(size);f.setBold(bold);p.setFont(f);p.setPen(QColor(color));p.drawText(QPointF(x,y),s);};
        QLinearGradient header(0,0,0,112);header.setColorAt(0,QColor("#26343f"));header.setColorAt(1,QColor("#111b22"));p.fillRect(QRectF(0,0,1600,112),header);
        label(24,40,"SA3000",29,"#eef6ff",true);line(178,18,178,47,"#617586");label(200,38,"Standalone Control",21,"#c9d7e3",true);
        line(432,18,432,47,"#617586");label(452,37,"Signal & Spectrum Analyzer",18,"#a8bac8");label(754,37,"RFSoC / ZCU111",17,"#a8bac8");label(1375,35,"NATIVE C++ / QT",12,"#7f9bab",true);
        line(0,58,1600,58,"#43535f");line(0,111,1600,111,"#617382");
        box(0,113,100,720,"#26343e","#536572",4);box(1040,113,560,720,"#293740","#536572",6);
        box(1052,122,539,242,"#172127","#3a4b56",5);box(1052,375,539,447,"#172127","#3a4b56",5);
        label(1065,148,"FREQUENCY / SPAN / AMPLITUDE",13,"#a8bac8",true);label(1398,148,"MARKER / RUN",13,"#a8bac8",true);line(1387,138,1387,349,"#536774");
        label(1068,399,"NUMERIC ENTRY",14,"#a8bac8",true);box(921,124,113,698,"#080f14","#172a36",4);
        for(int i=0;i<4;++i){const double radii[]={78,70,60,56};const char *top[]={"#d5e2ed","#1b5487","#e5edf2","#b5c2cb"};const char *bottom[]={"#52697a","#0a243f","#788d9c","#748895"};
            const auto r=radii[i];QLinearGradient g(1452-r,485-r,1452+r,485+r);g.setColorAt(0,QColor(top[i]));g.setColorAt(1,QColor(bottom[i]));p.setBrush(g);p.setPen(QPen(QColor("#0b151d"),2));p.drawEllipse(QPointF(1452,485),r,r);}
        for(int i=0;i<16;++i){const auto a=i*3.141592653589793/8;p.setPen(QPen(QColor("#2d4559"),4));p.drawLine(QPointF(1452+70*std::cos(a),485+70*std::sin(a)),QPointF(1452+77*std::cos(a),485+77*std::sin(a)));}
        label(1398,589,"SCROLL TO ADJUST",11,"#a8bac8");
        line(0,840,1600,840,"#455d6d");line(472,853,472,881,"#5c788a");line(814,853,814,881,"#5c788a");
    }
};
inline void saButtonStyle(QPushButton *b,const QString &kind,double scale,int px=14){
    QString top="#344651",bottom="#1d2a32",edge="#69808e",ink="#e8f0f7";
    if(kind=="blue"){top="#079ff1";bottom="#0073c6";edge="#38c2ff";ink="#ffffff";}
    else if(kind=="light"){top="#d4d8d9";bottom="#a9b1b5";edge="#e4e9eb";ink="#10171c";}
    else if(kind=="yellow"){top="#ffdc28";bottom="#ffb800";edge="#ffe875";ink="#151c22";}
    else if(kind=="soft"){top="#48647e";bottom="#30475c";edge="#6d8ba3";ink="#f2f6fa";}
    b->setProperty("saKind",kind);b->setProperty("saPx",px);
    b->setStyleSheet(QString("QPushButton{color:%1;font-size:%2px;font-weight:600;border:1px solid %3;border-radius:5px;padding:0;background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 %4,stop:1 %5);}QPushButton:enabled:hover{border:1px solid #53c4ff;}QPushButton:enabled:pressed{background:#007fc2;color:white;}QPushButton:disabled{color:#77848f;border:1px solid #3c4852;background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #2b353d,stop:1 #202a31);}").arg(ink).arg(std::max(10,int(px*scale))).arg(edge,top,bottom));
}
