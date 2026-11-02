#include "core.hpp"
#include "control.hpp"
#include "latest.hpp"
#include "sa3000_skin.hpp"
#include "rx_power_qt.hpp"
#include "sweep.hpp"
#include "spectrum_tools.hpp"
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QHeaderView>
#include <QtGui/QPolygonF>
#include "tx_power_qt.hpp"
#include <optional>
#include <chrono>
#include "tx.hpp"
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QDialog>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QAbstractSpinBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QScrollArea>
#include <QtCore/QPointer>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QProgressBar>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QMouseEvent>
#include <QtGui/QCloseEvent>
#include <QtGui/QFontDatabase>
#include <QtGui/QCursor>
#include <QtGui/QPixmap>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QStandardPaths>
#include <QtCore/QDateTime>
#include <QtCore/QTimer>
#include <QtCore/QSettings>
#include <QtCore/QUuid>
#include <QtCore/QElapsedTimer>
#include <QtCore/QLockFile>
#include <QtCore/QSignalBlocker>
#include <algorithm>
#include <cmath>
#include <thread>
#include <functional>
#include <csignal>

using namespace instrument;
namespace {
std::atomic_bool terminateRequested{false};
static_assert(std::atomic_bool::is_always_lock_free,"Signal flag must be lock free");
void terminateHandler(int){terminateRequested.store(true);}
fs::path path(const QString &text) {return fs::u8path(text.toUtf8().constData());}
QString text(const fs::path &value) {return QString::fromUtf8(value.u8string().c_str());}
QString errorText(const std::exception &e) {return QString::fromUtf8(e.what());}
QCursor instrumentCursor() {
    // The minimal board image may have no Xcursor theme. A pixmap cursor avoids
    // theme lookup, while Weston still positions the pointer from real input.
    QPixmap image(32,40);image.fill(Qt::transparent);
    QPainter painter(&image);painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath arrow;arrow.moveTo(3,2);arrow.lineTo(3,29);arrow.lineTo(10,23);
    arrow.lineTo(16,36);arrow.lineTo(21,33);arrow.lineTo(15,21);arrow.lineTo(25,21);arrow.closeSubpath();
    painter.setPen(QPen(Qt::black,2,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
    painter.setBrush(Qt::white);painter.drawPath(arrow);painter.end();
    return QCursor(image,3,2);
}
const char *style=R"(
QWidget{background:#10191f;color:#e8f0f7;font-family:"DejaVu Sans";font-size:14px}
QLabel{border:0;background:transparent} QLabel#source{background:#0067b6;color:#ffffff;font-size:13px;font-weight:bold;padding:3px 8px}
QGroupBox{border:1px solid #3a4b56;border-radius:5px;margin-top:12px;padding:14px 10px 10px;background:#111c23}
QGroupBox::title{subcontrol-origin:margin;left:12px;color:#a8bac8}
QPushButton{color:#e8f0f7;background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #344651,stop:1 #1d2a32);border:1px solid #69808e;border-radius:5px;padding:8px}
QPushButton:enabled:hover{border-color:#53c4ff} QPushButton:enabled:pressed{background:#007fc2}
QPushButton:disabled{color:#77848f;background:#202a31;border-color:#3c4852}
QComboBox,QSpinBox,QDoubleSpinBox,QPlainTextEdit,QLineEdit{background:#1d2a32;color:#e8f0f7;border:1px solid #69808e;border-radius:4px;padding:5px;selection-background-color:#0073c6}
QComboBox:focus,QSpinBox:focus,QDoubleSpinBox:focus{border:1px solid #38c2ff}
QComboBox:disabled,QSpinBox:disabled,QDoubleSpinBox:disabled{color:#77848f;background:#202a31;border-color:#3c4852}
QComboBox QAbstractItemView,QMenu{background:#1d2a32;color:#e8f0f7;selection-background-color:#0073c6}
QLabel#metric{font-size:15px;color:#38c2ff;font-weight:bold}
QProgressBar{background:#0c161d;border:1px solid #18a9ed;border-radius:3px;text-align:center;font-size:11px}
QProgressBar::chunk{background:#009cef} QToolTip{background:#1d2a32;color:#e8f0f7;border:1px solid #69808e}
)";

class Plot final:public QWidget {
public:
    std::uint64_t dataSerial=0,paintSerial=0,paintedFrames=0;
    Measurement data;bool hasData=false,live=false,holdOn=false,markerOn=true;
    bool swept=false;std::vector<double> rfGrid;QString sweepSummary,traceCaption;
    double span=150e6,reference=0,range=120,markerHz=0,markerPower=-300;
    std::vector<double> hold;MarkerBank markers;
    std::function<void(double,double)> markerChanged;
    explicit Plot(QWidget *parent=nullptr):QWidget(parent){setMinimumSize(520,280);}
    QRectF area()const{return QRectF(66,28,std::max(1,width()-90),std::max(1,height()-77));}
    void clearHold(){hold.clear();update();}
    void setData(Measurement value,bool isLive,std::vector<double> explicitGrid={}){
        if(!hasData||value.n!=data.n||isLive!=live||value.center!=data.center||value.fs!=data.fs)hold.clear();
        swept=!explicitGrid.empty();rfGrid=std::move(explicitGrid);traceCaption.clear();
        ++dataSerial;data=std::move(value);hasData=true;live=isLive;
        if(holdOn){if(hold.empty())hold=data.displayDbfs;else for(unsigned k=0;k<data.n;++k)hold[k]=std::max(hold[k],data.displayDbfs[k]);}
        refreshMarkers();
    }
    void setSweep(const SweepResult &result){
        Measurement m;m.n=unsigned(result.dbfs.size());m.frames=result.averages;m.center=(result.start+result.stop)/2;m.fs=result.stop-result.start;m.spacing=result.spacing;m.enbwHz=result.enbw;m.displayDbfs=result.dbfs;
        m.peak=unsigned(std::max_element(m.displayDbfs.begin(),m.displayDbfs.end())-m.displayDbfs.begin());m.peakDbfs=m.displayDbfs[m.peak];m.peakHz=result.frequency[m.peak];
        setData(std::move(m),true,result.frequency);
        sweepSummary=QString("SWEPT / %1 segments / actual %2s / %3").arg(result.centers.size()).arg(result.seconds,0,'f',3).arg(result.targetSeconds>0?QString("minimum %1s").arg(result.targetSeconds,0,'f',3):"Auto");update();
    }
    void notifyMarkers(){const auto &m=markers.points[markers.active];markerHz=m.valid?m.frequency:0;markerPower=m.valid?m.level:-300;if(markerChanged)markerChanged(markerHz,markerPower);update();}
    void refreshMarkers(){if(!hasData)return;if(rfGrid.empty()){rfGrid.resize(data.n);for(unsigned k=0;k<data.n;++k)rfGrid[k]=data.center-data.fs/2+k*data.spacing;}markers.setTrace(rfGrid,data.displayDbfs,data.center-span/2,data.center+span/2);notifyMarkers();}
    void viewChanged(){if(hasData){markers.setView(data.center-span/2,data.center+span/2);notifyMarkers();}else update();}
    void peak(){if(hasData){markers.setView(data.center-span/2,data.center+span/2);markers.peak(markers.active);notifyMarkers();}}
    void fourPeaks(){if(hasData){markers.setView(data.center-span/2,data.center+span/2);const auto n=markers.fourPeaks(std::max(2*data.enbwHz,3*data.spacing));markers.active=n>1?1:0;markers.reference=0;markers.deltaEnabled=n>1;notifyMarkers();}}

protected:
    void paintEvent(QPaintEvent*)override {
        if(paintSerial!=dataSerial){paintSerial=dataSerial;++paintedFrames;}
        QPainter p(this);p.fillRect(rect(),QColor("#080f14"));p.setFont(QFont("DejaVu Sans",9));const auto a=area();
        const auto center=hasData?data.center:1.8e9,low=center-span/2;
        for(int k=0;k<=10;++k){const auto x=a.left()+a.width()*k/10;p.setPen(QColor("#394750"));p.drawLine(QPointF(x,a.top()),QPointF(x,a.bottom()));if(k%2==0){p.setPen(QColor("#9caebe"));p.drawText(QRectF(x-45,a.bottom()+5,90,20),Qt::AlignCenter,QString::number((low+span*k/10)/1e6,'f',3));}}
        for(int k=0;k<=8;++k){const auto y=a.top()+a.height()*k/8;p.setPen(QColor("#394750"));p.drawLine(QPointF(a.left(),y),QPointF(a.right(),y));p.setPen(QColor("#9caebe"));p.drawText(QRectF(0,y-10,60,20),Qt::AlignRight|Qt::AlignVCenter,QString::number(reference-range*k/8,'f',0));}
        p.setPen(QColor("#9caebe"));p.drawText(8,17,"dBFS / bin");p.drawText(int(a.center().x()-28),height()-6,"RF / MHz");
        if(!hasData){p.drawText(a,Qt::AlignCenter,"Select a ready device or an explicit replay record, then SINGLE");return;}
        p.drawText(int(a.left()+8),int(a.top()+18),swept?sweepSummary:(!traceCaption.isEmpty()?traceCaption:(live?"LAST VALIDATED LIVE SNAPSHOT":"REPLAY / SAVED BOARD DATA")));
        const auto start=swept?int(std::lower_bound(rfGrid.begin(),rfGrid.end(),low)-rfGrid.begin()):std::max(0,int((low-data.center+data.fs/2)/data.fs*data.n));
        const auto end=swept?int(std::upper_bound(rfGrid.begin(),rfGrid.end(),low+span)-rfGrid.begin()):std::min(int(data.n),int(std::ceil((low+span-data.center+data.fs/2)/data.fs*data.n))+1);
        const auto stride=std::max(1,int(std::ceil((end-start)/a.width())));p.save();p.setClipRect(a);
        auto curve=[&](const std::vector<double> &values,QColor color){
            if(values.empty())return;
            QPainterPath line;bool first=true;p.setPen(QPen(color,1.05));
            for(int k=start;k<end;k+=stride){auto bounds=std::minmax_element(values.begin()+k,values.begin()+std::min(end,k+stride));
                const auto x=a.left()+((swept?rfGrid[std::size_t(k)]:data.center-data.fs/2+k*data.spacing)-low)/span*a.width();
                const auto top=a.top()+(reference-*bounds.second)/range*a.height(),bottom=a.top()+(reference-*bounds.first)/range*a.height();
                p.drawLine(QPointF(x,top),QPointF(x,bottom));if(first){line.moveTo(x,top);first=false;}else line.lineTo(x,top);}
            p.drawPath(line);
        };
        if(holdOn)curve(hold,QColor("#467e69"));
        curve(data.displayDbfs,QColor("#ffe927"));
        const QColor colors[]={QColor("#52e1cf"),QColor("#ff82dc"),QColor("#72b7ff"),QColor("#ffb34b")};
        if(markerOn)for(unsigned id=0;id<4;++id){const auto &m=markers.points[id];if(!m.enabled||!m.valid||m.frequency<low||m.frequency>low+span)continue;
            const double x=a.left()+(m.frequency-low)/span*a.width(),y=std::clamp(a.top()+(reference-m.level)/range*a.height(),a.top()+12,a.bottom());
            p.setPen(QPen(colors[id],id==markers.active?2:1,Qt::DashLine));p.drawLine(QPointF(x,a.top()),QPointF(x,a.bottom()));p.setBrush(colors[id]);
            QPolygonF triangle;triangle<<QPointF(x-4,y-9)<<QPointF(x+4,y-9)<<QPointF(x,y-2);p.drawPolygon(triangle);p.drawText(QPointF(std::clamp(x+5,a.left(),a.right()-25),a.top()+38+14*id),QString("M%1").arg(id+1));}

        p.restore();
    }
    void mousePressEvent(QMouseEvent *event)override {
        if(!hasData)return;
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
        const auto pos=event->position();
#else
        const auto pos=event->localPos();
#endif
        const auto a=area();if(!a.contains(pos))return;const auto f=data.center-span/2+(pos.x()-a.left())/a.width()*span;
        markers.position(markers.active,f);notifyMarkers();
    }
};

class History final:public QWidget {
    std::vector<std::array<QRgb,512>> rows;
public:
    explicit History(QWidget *parent=nullptr):QWidget(parent){setMinimumHeight(120);}
    void clear(){rows.clear();update();}
    void append(const Measurement &m,double reference,double range){
        std::array<QRgb,512> row{};
        for(unsigned x=0;x<512;++x){const auto lo=x*m.n/512,hi=(x+1)*m.n/512;
            const auto peak=*std::max_element(m.displayDbfs.begin()+lo,m.displayDbfs.begin()+std::max(lo+1,hi));
            const auto t=std::clamp((peak-reference+range)/range,0.,1.);row[x]=QColor::fromHsvF(.68*(1-t),.9,.12+.88*t).rgb();}
        rows.insert(rows.begin(),row);if(rows.size()>80)rows.resize(80);update();
    }
protected:
    void paintEvent(QPaintEvent*)override{QPainter p(this);p.fillRect(rect(),QColor("#080f14"));if(!rows.empty()){QImage image(512,int(rows.size()),QImage::Format_RGB32);for(std::size_t y=0;y<rows.size();++y)for(int x=0;x<512;++x)image.setPixel(x,int(y),rows[y][std::size_t(x)]);p.drawImage(rect(),image);}p.setPen(QColor("#dceaf5"));p.drawText(9,18,"SNAPSHOT HISTORY / FULL 150 MHz / NOT GAPLESS IQ");}
};

class Window final:public QMainWindow {
    fs::path resourcesPath;Resources resources;
    QString outputPath;
    QComboBox *source=nullptr,*fft=nullptr,*waveType=nullptr,*order=nullptr,*vbwMode=nullptr;
    QDoubleSpinBox *vbwHz=nullptr;QLabel *vbwNote=nullptr;
    QDialog *markerDialog=nullptr;QComboBox *activeMarker=nullptr,*referenceMarker=nullptr;QCheckBox *markerEnable=nullptr,*markerTrack=nullptr,*deltaMode=nullptr,*markerVisible=nullptr;QDoubleSpinBox *markerFrequency=nullptr;QTableWidget *markerTable=nullptr;
    std::array<QPushButton*,4> markerCards{};QPushButton *mainDelta=nullptr,*mainReference=nullptr;
    QSpinBox *averages=nullptr,*peakCounts=nullptr;
    QDoubleSpinBox *rxCenter=nullptr;
    unsigned sweepShown=0;
    QComboBox *powerMode=nullptr,*powerUnit=nullptr,*powerContext=nullptr,*sweepMode=nullptr;QDoubleSpinBox *targetPower=nullptr,*sweepSeconds=nullptr;
    QLabel *powerNote=nullptr,*sweepTimeNote=nullptr;QWidget *txParameters=nullptr;QDialog *txAdvanced=nullptr;
    std::optional<TxCwCalibration> txCalibration,txContextCalibration;QString txCalibrationError,txContextError;bool txValidatedForControls=false;
    QDoubleSpinBox *span=nullptr,*ref=nullptr,*yrange=nullptr,*offset=nullptr,*rfFrequency=nullptr,*pulseWidth=nullptr,*pulsePeriod=nullptr,*chirpWidth=nullptr,*symbolRate=nullptr,*rolloff=nullptr;
    QPushButton *single=nullptr,*repeat=nullptr,*stop=nullptr,*open=nullptr,*generate=nullptr,*directory=nullptr,*txUpload=nullptr,*txPlay=nullptr,*txStop=nullptr;
    QLabel *txHealth=nullptr,*waveFile=nullptr;QString lastWaveform;std::shared_ptr<TxSession> tx;std::thread txStopTask;std::uint32_t txFlags=0;bool txBusy=false;
    QCheckBox *hold=nullptr;
    Plot *plot=nullptr;History *history=nullptr;
    QLabel *badge=nullptr,*marker=nullptr,*df=nullptr,*enbw=nullptr,*tone=nullptr,*power=nullptr,*age=nullptr;
    QPlainTextEdit *log=nullptr,*health=nullptr;QProgressBar *progress=nullptr;
    std::thread task;std::atomic_bool cancelled{false};bool busy=false,liveFault=false,closing=false;RepeatGate repeatGate;
    struct StreamUpdate {CaptureResult result;std::uint64_t batches=0;};
    LatestFrame<StreamUpdate> streamLatest;bool streaming=false;
    QElapsedTimer streamElapsed;std::uint64_t streamDisplayed=0,streamPaintBase=0;int streamLogSecond=-5;bool liveStreamTest=false;
    std::vector<fs::path> replay;
    QElapsedTimer resultAge;bool lastLive=false;QString lastError,lastDataPath;
    bool autoTest=false;QString testScreenshot;
    std::optional<RxCwCalibration> rxCalibration;QString rxCalibrationFile,rxCalibrationError;bool rxCalibrationEnabled=true;QLabel *cwInput=nullptr;
    SaCanvas *canvas=nullptr;QStackedWidget *pages=nullptr;QDialog *rxSettings=nullptr;
    struct Position {QWidget *widget;QRectF rect;};std::vector<Position> positioned;
    std::vector<QPushButton*> nav,softKeys,padKeys,unitKeys;std::vector<QPushButton*> functionKeys;
    QLabel *context=nullptr,*connection=nullptr,*eventLine=nullptr,*rxState=nullptr,*rfState=nullptr,*entryLabel=nullptr,*meta=nullptr;
    QPushButton *multiView=nullptr,*selectIq=nullptr;QPointer<QAbstractSpinBox> activeEntry;
    QString entryBuffer;bool entryPending=false,receiving=false;int currentPage=0;

public:
    explicit Window(fs::path resourceRoot,QString work,bool selfTest=false,QString screenshot={},bool generatorTest=false,bool streamTest=false,bool uploadTest=false,bool sweepTest=false,bool liveSweepTest=false,bool powerTest=false,bool manualSweepTest=false,bool markerTest=false,int videoTestMode=0):
        resourcesPath(std::move(resourceRoot)),resources(loadResources(resourcesPath)),outputPath(std::move(work)),autoTest(selfTest),testScreenshot(std::move(screenshot)) {
        setWindowTitle("SA3000 / ZCU111 / Standalone Signal Instrument");resize(1600,900);
        canvas=new SaCanvas;setCentralWidget(canvas);statusBar()->hide();
        pages=new QStackedWidget(canvas);place(pages,{111,169,806,647});
        pages->addWidget(rxPage());pages->addWidget(txPage());pages->addWidget(systemPage());
        createFrontPanel();
        progress=new QProgressBar(canvas);progress->setRange(0,100);progress->setValue(0);place(progress,{1007,856,235,19});
        age=new QLabel("No validated acquisition yet",canvas);age->setToolTip("No saved/live record yet");place(age,{20,853,440,30});
        eventLine=new QLabel("Ready / RF not started",canvas);place(eventLine,{494,853,310,30});
        rxState=new QLabel("RX STOPPED",canvas);place(rxState,{836,853,165,30});
        rfState=new QLabel("RF STATUS UNKNOWN",canvas);place(rfState,{1260,853,320,30});
        connection=new QLabel("Linux backend",canvas);place(connection,{1005,70,238,30});
        canvas->resized=[this]{layoutFrontPanel();};canvas->dial=[this](int direction){stepEntry(direction);};
        connect(qApp,&QApplication::focusChanged,this,[this](QWidget*,QWidget *now){
            QWidget *parent=now;while(parent&&!qobject_cast<QAbstractSpinBox*>(parent))parent=parent->parentWidget();
            if(auto *spin=qobject_cast<QAbstractSpinBox*>(parent)){activeEntry=spin;entryPending=false;entryBuffer.clear();updateEntry();}
        });
        layoutFrontPanel();switchPage(0);
        auto *timer=new QTimer(this);connect(timer,&QTimer::timeout,this,[this]{if(resultAge.isValid())age->setText(QString("%1 | last result age %2 s | %3").arg(lastLive?"LIVE SNAPSHOT":"REPLAY").arg(resultAge.elapsed()/1000).arg(lastDataPath));});timer->start(1000);
        auto *txTimer=new QTimer(this);connect(txTimer,&QTimer::timeout,this,[this]{
            if(!busy&&!txStopTask.joinable()&&txAvailable()){
                try{showTx(tx?tx->info():txInspect());}catch(const std::exception&e){txFlags=0;txHealth->setText("TX status error: "+errorText(e));updateTxButtons();}
            }
        });txTimer->start(1000);
        auto *streamTimer=new QTimer(this);connect(streamTimer,&QTimer::timeout,this,[this]{pollStream();});streamTimer->start(100);
        liveStreamTest=streamTest||videoTestMode==1;refreshHealth();loadRxCalibration(text(resourcesPath/"rx_cw_calibration.json"));
        if(txAvailable()){try{showTx(txInspect());}catch(const std::exception &e){message("TX status unavailable: "+errorText(e));}}
        if(autoTest)QTimer::singleShot(100,this,[this,generatorTest,streamTest,uploadTest,sweepTest,liveSweepTest,powerTest,manualSweepTest,markerTest,videoTestMode]{
            if(videoTestMode){fft->setCurrentIndex(1);averages->setValue(1);span->setValue(150);vbwMode->setCurrentIndex(1);vbwHz->setValue(1000);if(acquisitionFrames()!=12){QCoreApplication::exit(2);return;}if(videoTestMode==1){startStream();QTimer::singleShot(8000,this,[this]{cancelled.store(true);});}else start(false);return;}
            if(markerTest){
                fft->setCurrentIndex(1);vbwMode->setCurrentIndex(1);vbwHz->setValue(1000);if(videoPlan().frames!=12){QCoreApplication::exit(2);return;}averages->setValue(16);if(videoPlan().frames!=16){QCoreApplication::exit(2);return;}averages->setValue(1);vbwMode->setCurrentIndex(0);
                Measurement m;m.n=8192;m.frames=1;m.spacing=150e6/8192;m.enbwHz=videoPlan().rbwEnbw;m.displayDbfs.assign(m.n,-100);m.peakDbfs=-20;m.peakHz=m.center-m.fs/2+1200*m.spacing;
                m.displayDbfs[1200]=-20;m.displayDbfs[2800]=-30;m.displayDbfs[4600]=-40;m.displayDbfs[6800]=-50;plot->setData(m,false);plot->fourPeaks();const auto d=plot->markers.delta(1);
                if(!d.valid||d.level!=-10||std::abs(d.frequency-1600*m.spacing)>.001){QCoreApplication::exit(2);return;}
                plot->markers.position(1,plot->markers.points[1].frequency);plot->setData(m,false);if(plot->markers.points[1].tracking){QCoreApplication::exit(2);return;}
                for(unsigned i=0;i<4;++i)if(!markerCards[i]->isVisible()||!markerCards[i]->text().contains(QString::number(plot->markers.points[i].frequency/1e6,'f',6))||!markerCards[i]->text().contains("dBFS/bin")){message("MAIN_MARKER_VALUES_FAILED");QCoreApplication::exit(2);return;}
                if(!marker->text().contains("-10.000 dB")){QCoreApplication::exit(2);return;}
                auto changed=m;changed.displayDbfs[2800]=-31;plot->setData(changed,false);if(!markerCards[1]->text().contains("-31.00 dBFS/bin")||!marker->text().contains("-11.000 dB")){QCoreApplication::exit(2);return;}plot->setData(m,false);
                plot->markers.enable(2,false);plot->notifyMarkers();if(!markerCards[2]->text().contains("OFF")){QCoreApplication::exit(2);return;}markerCards[2]->click();if(markerDialog||plot->markers.active!=2||!plot->markers.points[2].enabled){QCoreApplication::exit(2);return;}
                mainDelta->click();if(plot->markers.deltaEnabled||mainDelta->text()!="Delta OFF"){QCoreApplication::exit(2);return;}mainDelta->click();mainReference->click();if(plot->markers.reference!=1){QCoreApplication::exit(2);return;}plot->markers.reference=0;selectMarker(1);message("MAIN_MARKER_CARDS_REFRESH_SELECT_DELTA_PASS");
                plot->traceCaption="SYNTHETIC FOUR-TONE UI TEST / NO RF DATA";badge->setText("SYNTHETIC FOUR-TONE MARKER TEST / NO RF DATA");ref->setValue(-10);yrange->setValue(100);refreshCapabilities();showMarkerSettings();
                QTimer::singleShot(200,this,[this,m]{const double original=plot->markers.points[1].frequency;markerFrequency->setFocus();auto *edit=markerFrequency->findChild<QLineEdit*>();edit->setText("1780.123");edit->setModified(true);const auto typed=edit->text();plot->setData(m,false);if(edit->text()!=typed){message("MARKER_FOCUS_EDIT_FAILED before="+typed+" after="+edit->text());QCoreApplication::exit(2);return;}markerFrequency->clearFocus();edit->setModified(false);plot->markers.position(1,original);plot->notifyMarkers();message("MARKER_PENDING_EDIT_PRESERVED_PASS");plot->traceCaption="SYNTHETIC FOUR-TONE UI TEST / NO RF DATA";if(!testScreenshot.isEmpty()&&!markerDialog->grab().save(testScreenshot+".markers.png")){QCoreApplication::exit(2);return;}markerDialog->hide();QTimer::singleShot(80,this,[this]{const bool ok=testScreenshot.isEmpty()||grab().save(testScreenshot);QCoreApplication::exit(ok?0:2);});});return;
            }
            if(powerTest){switchPage(1);if(!modelPowerReady()||requestedDbm()!=-25||powerContext->currentIndex()!=1){QCoreApplication::exit(2);return;}QTimer::singleShot(200,this,[this]{QCoreApplication::exit(testScreenshot.isEmpty()||grab().save(testScreenshot)?0:2);});return;}
            if(uploadTest){
                switchPage(1);waveType->setCurrentText("CW");makeWave();
                auto *check=new QTimer(this);auto stage=std::make_shared<int>(0);auto elapsed=std::make_shared<QElapsedTimer>();elapsed->start();
                connect(check,&QTimer::timeout,this,[this,check,stage,elapsed]{
                    if(elapsed->elapsed()>90000){check->stop();cancelled.store(true);fail("TX GUI upload acceptance timed out");return;}
                    if(busy)return;
                    if(*stage==0&&!lastWaveform.isEmpty()){*stage=1;uploadTx();}
                    else if(*stage==1){check->stop();if((txFlags&7)!=3){fail("TX GUI upload did not finish loaded and stopped");return;}
                        message("SA3000_TX_GUI_GENERATE_UPLOAD_PASS; no RF playback requested");
                        QTimer::singleShot(150,this,[this]{const bool ok=testScreenshot.isEmpty()||grab().save(testScreenshot);QCoreApplication::exit(ok?0:2);});}
                });check->start(200);return;
            }
            if(liveSweepTest||manualSweepTest){
                fft->setCurrentIndex(1);averages->setValue(1);span->setValue(1000);if(manualSweepTest){sweepMode->setCurrentIndex(1);sweepSeconds->setValue(1);}startSweep(true);
                QTimer::singleShot(8000,this,[this]{requestRxStop();auto *done=new QTimer(this);connect(done,&QTimer::timeout,this,[this,done]{if(busy)return;done->stop();message(QString("LIVE_SWEEP_UI_TEST complete_rounds=%1").arg(sweepShown));const bool ok=sweepShown>=3&&(testScreenshot.isEmpty()||grab().save(testScreenshot));QCoreApplication::exit(ok?0:2);});done->start(100);});return;
            }
            if(sweepTest){
                SweepResult result;result.start=1.3e9;result.stop=2.3e9;const auto centers=planSweep(result.start,result.stop);
                for(unsigned i=0;i<centers.size();++i){Measurement m;m.n=8192;m.frames=1;m.center=double(centers[i]);m.spacing=m.fs/m.n;m.enbwHz=1.5*m.spacing;m.displayDbfs.assign(m.n,-110);
                    for(unsigned k=0;k<m.n;++k){const auto f=m.center-m.fs/2+k*m.spacing;for(double peak:{1.3e9,1.79e9,2.3e9})if(std::abs(f-peak)<2*m.spacing)m.displayDbfs[k]=-40;}
                    const double low=i?(double(centers[i-1])+centers[i])/2:result.start,high=i+1<centers.size()?(double(centers[i])+centers[i+1])/2:result.stop;
                    appendSweepSlice(result,m,low,high,i+1==centers.size());result.centers.push_back(double(centers[i]));}
                result.complete=true;result.seconds=1.234;fft->setCurrentIndex(1);span->setValue(1000);ref->setValue(-30);plot->setSweep(result);plot->sweepSummary="SYNTHETIC SWEEP DISPLAY TEST / NO HARDWARE";refreshCapabilities();
                if(!functionKeys[0]->isEnabled()||plot->rfGrid.empty()||!plot->swept){QCoreApplication::exit(2);return;}
                QTimer::singleShot(200,this,[this]{const bool ok=testScreenshot.isEmpty()||grab().save(testScreenshot);QCoreApplication::exit(ok?0:2);});return;
            }
            if(streamTest){fft->setCurrentIndex(2);startStream();QTimer::singleShot(8000,this,[this]{cancelled.store(true);});return;}
            if(generatorTest){
                switchPage(1);
                if(!modelPowerReady()||targetPower->value()!=-25){QCoreApplication::exit(2);return;}
                powerUnit->setCurrentIndex(1);if(targetPower->value()!=-55||requestedDbm()!=-25){QCoreApplication::exit(2);return;}
                powerUnit->setCurrentIndex(0);if(targetPower->value()!=-25){QCoreApplication::exit(2);return;}
                powerContext->setCurrentIndex(0);targetPower->setValue(-45);txValidatedForControls=true;powerContext->setCurrentIndex(1);
                if(txValidatedForControls||targetPower->value()!=-40||targetPower->minimum()!=-40||targetPower->maximum()!=-20){QCoreApplication::exit(2);return;}
                targetPower->setValue(-25);
                txValidatedForControls=true;rfFrequency->setValue(1800);if(txValidatedForControls||generate->isEnabled()){QCoreApplication::exit(2);return;}
                waveType->setCurrentText("QAM");order->setCurrentText("64");
                activeEntry=rfFrequency;entryPending=false;
                for(const auto &digit:QStringList{"1",".","8","0","1","5"})appendEntry(digit);
                commitEntry(0);
                if(rfFrequency->value()!=1801.5||offset->value()!=-1.5||functionKeys[7]->isEnabled()){QCoreApplication::exit(2);return;}
                rfFrequency->setValue(1800);
                if(rfFrequency->value()!=1800||offset->value()!=0||!symbolRate->isEnabled()||pulseWidth->isEnabled()||txPlay->isEnabled()) {QCoreApplication::exit(2);return;}
                QTimer::singleShot(100,this,[this]{bool ok=testScreenshot.isEmpty()||grab().save(testScreenshot);QCoreApplication::exit(ok?0:2);});return;
            }
            source->setCurrentIndex(1);fft->setCurrentIndex(1);start(false);
        });
    }
    void selectPage(int page){switchPage(page);}
    void configureSweepView(){source->setCurrentIndex(0);fft->setCurrentIndex(1);averages->setValue(1);span->setValue(1000);rxCenter->setValue(1800);message("1GHz swept view configured; press RUN to acquire. No RF output started.");refreshCapabilities();}
    ~Window()override{QObject::disconnect(qApp,nullptr,this,nullptr);if(markerDialog)markerDialog->hide();activeEntry.clear();cancelled.store(true);if(task.joinable())task.join();if(txStopTask.joinable())txStopTask.join();tx.reset();}
private:
    QPushButton *button(QVBoxLayout *layout,const QString &label,const std::function<void()> &action){auto *b=new QPushButton(label);layout->addWidget(b);connect(b,&QPushButton::clicked,this,action);return b;}
    void place(QWidget *widget,QRectF rect){positioned.push_back({widget,rect});}
    void layoutFrontPanel(){
        for(const auto &item:positioned){item.widget->setGeometry(canvas->map(item.rect));
            if(auto *b=qobject_cast<QPushButton*>(item.widget))saButtonStyle(b,b->property("saKind").toString(),canvas->scale(),b->property("saPx").toInt());}
    }
    QPushButton *frontButton(QString label,QRectF rect,QString kind="dark",std::function<void()> action={},int px=14){
        auto *b=new QPushButton(label,canvas);b->setFocusPolicy(Qt::NoFocus);saButtonStyle(b,kind,canvas->scale(),px);place(b,rect);
        b->setEnabled(bool(action));b->setToolTip(action?"":"Unavailable in this release");if(action)connect(b,&QPushButton::clicked,this,[action]{action();});return b;
    }
    void switchPage(int index){currentPage=index;pages->setCurrentIndex(index);
        for(int k=0;k<int(nav.size());++k)saButtonStyle(nav[std::size_t(k)],k==index?"blue":"dark",canvas->scale(),17);
        if(context)context->setText(index==1?"Generator":index==2?"System":"Frequency");
        activeEntry.clear();entryPending=false;entryBuffer.clear();refreshCapabilities();}
    void showSettings(QWidget *focus=nullptr){rxSettings->show();rxSettings->raise();if(focus){focus->setFocus();if(auto *s=qobject_cast<QAbstractSpinBox*>(focus)){activeEntry=s;s->selectAll();entryPending=false;updateEntry();}}}
    void requestRxStop(){repeatGate.stop();cancelled.store(true);message("RX stop requested; current frame completes without DMA reset.");refreshCapabilities();}
    void selectWaveFile(){if(busy)return;auto filename=QFileDialog::getOpenFileName(this,"Select native IQ + matching JSON",outputPath,"IQ (*.bin)");
        if(filename.isEmpty())return;
        if(!QFile::exists(filename+".json")){fail("Matching IQ .bin.json is required");return;}
        generatorEdited();powerMode->setCurrentIndex(1);lastWaveform=filename;waveFile->setText("Selected file / no control-panel power claim: "+filename);updateTxButtons();}
    void selectOutput(){if(busy)return;repeatGate.stop();auto value=QFileDialog::getExistingDirectory(this,"Writable output directory",outputPath);
        if(!value.isEmpty()){outputPath=value;QSettings().setValue("output",value);refreshHealth();}}
    void screenshot(){auto name=QFileDialog::getSaveFileName(this,"Save SA3000 screen",outputPath+"/sa3000.png","PNG (*.png)");if(!name.isEmpty()&&!grab().save(name))fail("Screenshot save failed");}
    void fileMenu(){QMenu menu(this);auto *record=menu.addAction("Open spectrum packet + receipt");record->setEnabled(!busy);
        auto *iq=menu.addAction("Select existing IQ + metadata");iq->setEnabled(!busy);auto *out=menu.addAction("Select output directory");out->setEnabled(!busy);auto *screen=menu.addAction("Save screenshot");
        auto *chosen=menu.exec(open->mapToGlobal(QPoint(open->width(),0)));if(chosen==record)selectReplay();else if(chosen==iq)selectWaveFile();else if(chosen==out)selectOutput();else if(chosen==screen)screenshot();}
    void about(){QMessageBox::information(this,"SA3000 / local controls","Native C++ Qt on ZCU111.\nSpectrum: Setup selects FFT / linear averaging / display scale. Run Single saves a full validated record; Run Cont. refreshes without per-frame archives.\nGenerator: Generate IQ, Upload to DDR4, then explicitly RF Play. Parameter edits require a new generation and upload.\nGray keys are unavailable. RX center1300..2300MHz. Span>150MHz uses sequential NCO scanning; full interval must lie within1300..2300MHz. Spectrum remains dBFS. Narrowband CW calibration is separate.\nSelect a numeric field, then use keypad + Enter or a frequency unit key. Scroll the dial to step. RX captures have dead time; no gapless IQ recording.");}
    void autoScale(){if(!plot->hasData)return;const auto peak=*std::max_element(plot->data.displayDbfs.begin(),plot->data.displayDbfs.end());ref->setValue(std::clamp(std::ceil(peak/10)*10+10.,-180.,40.));yrange->setValue(90);}
    void showMeasurements(){if(!plot->hasData)return;if(plot->swept){QMessageBox::information(this,"Swept spectrum",plot->sweepSummary+"\nSpectrum dBFS/bin; no simultaneous full-span power or tone estimate.");return;}QMessageBox::information(this,"Validated spectrum measurements",QString("FFT %1 / averages %2\nBin spacing: %3\nWindow ENBW: %4\nTone estimate: %5\nTotal power: %6\n%7\nRX dBm uncalibrated; not an absolute-frequency calibration.").arg(plot->data.n).arg(plot->data.frames).arg(df->text(),enbw->text(),tone->text(),power->text(),marker->text()));}
    void softAction(unsigned index){if(currentPage==1){if(index==0)waveType->setFocus();else if(index==1)rfFrequency->setFocus();else if(index==2)focusPower();else if(index==3)order->setFocus();else if(index==5)stopTx();}
        else if(currentPage==0){if(index==0)showSettings(source);else if(index==1)showSettings(fft);else if(index==2)showSettings(averages);else if(index==3)showSettings(span);else if(index==4)showSettings(ref);else if(index==5)requestRxStop();}}
    void createFrontPanel(){
        const QStringList titles={"Preset","Setup","Print","File","Mode"};
        for(int k=0;k<5;++k){auto *b=frontButton(titles[k],{16.,128.+86*k,70,73},"dark",k==1?std::function<void()>([this]{showSettings();}):k==3?std::function<void()>([this]{fileMenu();}):k==4?std::function<void()>([this]{switchPage(currentPage==1?0:1);}):std::function<void()>{},14);if(k==3)open=b;}
        frontButton("Help",{16,559,70,44},"dark",[this]{about();});frontButton("Hold",{16,613,70,44},"dark",[this]{hold->setChecked(!hold->isChecked());});
        frontButton("Clear",{16,667,70,44},"dark",[this]{clearHistory();});frontButton("Status",{16,721,70,44},"dark",[this]{switchPage(2);});frontButton("Shot",{16,775,70,44},"dark",[this]{screenshot();});
        multiView=frontButton("MultiView",{110,124,160,46},"dark",[this]{history->setVisible(!history->isVisible());});
        for(int k=0;k<3;++k)nav.push_back(frontButton(QStringList{"Spectrum","Generator","System / Log"}[k],{280.+185*k,124,175,46},k==0?"blue":"dark",[this,k]{switchPage(k);},17));
        context=new QLabel("Frequency",canvas);context->setAlignment(Qt::AlignCenter);place(context,{925,138,106,40});
        const int ys[]={183,274,365,456,645,735};for(unsigned k=0;k<6;++k)softKeys.push_back(frontButton("",{925.,double(ys[k]),106,82},"soft",[this,k]{softAction(k);},14));stop=softKeys[5];
        const QStringList labels={"Freq.","Span","Ampt. /\nScale","Auto\nScale","FFT /\nRBW","Average","Trace /\nHold","Trigger","Meas","Power\nCal.","Sweep\nTime","Input /\nOutput"};
        for(int k=0;k<12;++k){std::function<void()> action;
            if(k==0)action=[this]{if(currentPage==1)rfFrequency->setFocus();else showSettings(rxCenter);};else if(k==1)action=[this]{showSettings(span);};else if(k==2)action=[this]{if(currentPage==1)focusPower();else showSettings(ref);};
            else if(k==3)action=[this]{autoScale();};else if(k==4)action=[this]{showSettings(fft);};else if(k==5)action=[this]{showSettings(averages);};else if(k==6)action=[this]{hold->setChecked(!hold->isChecked());};else if(k==8)action=[this]{showMeasurements();};else if(k==9)action=[this]{showRxPowerCalibration();};else if(k==10)action=[this]{showSettings(sweepSeconds);};else if(k==11)action=[this]{if(currentPage==1)selectWaveFile();else showSettings(source);};
            functionKeys.push_back(frontButton(labels[k],{1065.+76*(k%4),162.+63*(k/4),67,51},"light",action,12));}
        auto *mark=frontButton("Marker",{1404,162,79,51},"light",[this]{showMarkerSettings();},12);mark->setProperty("needsTrace",true);
        auto *peak=frontButton("Peak\nSearch",{1492,162,79,51},"dark",[this]{plot->markerOn=true;plot->peak();},12);peak->setProperty("needsTrace",true);
        auto *markerFunction=frontButton("Marker\nFunction",{1404,225,79,51},"light",[this]{showMarkerSettings();},12);markerFunction->setProperty("needsTrace",true);
        auto *markerNext=frontButton("Marker >",{1492,225,79,51},"light",[this]{selectMarker((plot->markers.active+1)%4);},12);markerNext->setProperty("needsTrace",true);
        single=frontButton("Run\nSingle",{1404,288,79,51},"light",[this]{start(false);},12);repeat=frontButton("Run\nCont.",{1492,288,79,51},"yellow",[this]{start(true);},12);
        const QStringList digits={"7","8","9","4","5","6","1","2","3","0",".","-"};for(int k=0;k<12;++k)padKeys.push_back(frontButton(digits[k],{1068.+68*(k%3),418.+64*(k/3),59,51},"light",[this,k,digits]{appendEntry(digits[k]);},21));
        const QStringList units={"GHz","MHz","kHz","Hz"};for(int k=0;k<4;++k)unitKeys.push_back(frontButton(units[k],{1276.,418.+64*k,60,51},"dark",[this,k]{commitEntry(k);},14));
        padKeys.push_back(frontButton("Esc",{1068,680,88,54},"dark",[this]{entryPending=false;entryBuffer.clear();updateEntry();},17));
        padKeys.push_back(frontButton("Back",{1167,680,88,54},"dark",[this]{if(entryPending)entryBuffer.chop(1);updateEntry();},15));
        padKeys.push_back(frontButton("Enter",{1266,680,88,54},"blue",[this]{commitEntry(-1);},15));
        for(const auto &spec:std::vector<std::pair<QString,QRectF>>{{"Up",{1412,608,76,43}},{"Left",{1390,658,36,66}},{"Right",{1474,658,36,66}},{"Down",{1412,731,76,43}}})
            padKeys.push_back(frontButton(spec.first,spec.second,"dark",[this,spec]{stepEntry(spec.first=="Up"||spec.first=="Right"?1:-1);},11));
        frontButton("Undo",{1523,616,57,48},"light",{},12);frontButton("Redo",{1523,679,57,48},"light",{},12);
        frontButton("Screenshot",{1248,69,115,30},"dark",[this]{screenshot();},12);frontButton("Preset",{1370,69,92,30},"dark",{},12);frontButton("Help",{1470,69,105,30},"dark",[this]{about();},12);
        entryLabel=new QLabel("Select a numeric parameter",canvas);entryLabel->setWordWrap(true);place(entryLabel,{1068,744,286,66});
    }
    void updateEntry(){if(!entryLabel)return;const bool ready=activeEntry&&activeEntry->isEnabled();
        for(auto *b:padKeys)b->setEnabled(ready);
        for(auto *b:unitKeys)b->setEnabled(ready&&activeEntry->property("frequencyMHz").toBool());
        if(!ready){entryLabel->setText("Select a numeric parameter");return;}
        const auto name=activeEntry->accessibleName();entryLabel->setText(name+"\n"+(entryPending?entryBuffer+"  [Enter / unit]":activeEntry->findChild<QLineEdit*>()->text()));}
    void appendEntry(const QString &key){if(!activeEntry||!activeEntry->isEnabled())return;if(!entryPending){entryBuffer.clear();entryPending=true;}
        if(key=="-"){if(entryBuffer.startsWith('-'))entryBuffer.remove(0,1);else entryBuffer.prepend('-');}else if(key!="."||!entryBuffer.contains('.')){if(entryBuffer.size()<20)entryBuffer+=key;}updateEntry();}
    void commitEntry(int unit){if(!activeEntry||!activeEntry->isEnabled())return;if(!entryPending){activeEntry->interpretText();updateEntry();return;}
        bool ok=false;double value=entryBuffer.toDouble(&ok);if(unit>=0){const double scale[]={1000.,1.,.001,.000001};value*=scale[unit];}
        if(auto *d=qobject_cast<QDoubleSpinBox*>(activeEntry.data())){ok=ok&&std::isfinite(value)&&value>=d->minimum()&&value<=d->maximum();if(ok)d->setValue(value);}
        else if(auto *i=qobject_cast<QSpinBox*>(activeEntry.data())){ok=ok&&std::isfinite(value)&&value==std::floor(value)&&value>=i->minimum()&&value<=i->maximum();if(ok)i->setValue(int(value));}
        if(!ok){message("Numeric entry outside the selected parameter range; value not applied.");return;}entryPending=false;entryBuffer.clear();updateEntry();}
    void stepEntry(int direction){if(activeEntry&&activeEntry->isEnabled()){if(direction>0)activeEntry->stepUp();else activeEntry->stepDown();entryPending=false;updateEntry();}}
    QWidget *rxPage(){
        auto *page=new QWidget;auto *v=new QVBoxLayout(page);v->setContentsMargins(0,0,0,0);v->setSpacing(0);
        meta=new QLabel("ADC 2.4 GS/s / RX 150 MS/s    Center 1800 MHz fixed\nHann window / dBFS power / no gapless IQ");meta->setMargin(12);meta->setFixedHeight(64);v->addWidget(meta);
        badge=new QLabel("LOCAL DEVICE MODE / awaiting a validated capture");badge->setObjectName("source");badge->setFixedHeight(28);badge->setMinimumWidth(0);badge->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);v->addWidget(badge);
        auto *markerPanel=new QWidget;markerPanel->setObjectName("mainMarkerPanel");markerPanel->setFixedHeight(100);auto *markerGrid=new QGridLayout(markerPanel);markerGrid->setContentsMargins(4,4,4,2);markerGrid->setSpacing(4);
        for(unsigned i=0;i<4;++i){auto *card=new QPushButton(QString("M%1 / %2\n-- MHz / -- dBFS/bin").arg(i+1).arg(i?"OFF":"awaiting data"));card->setObjectName(QString("mainMarker%1").arg(i+1));card->setMinimumWidth(0);card->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Expanding);card->setFocusPolicy(Qt::NoFocus);card->setEnabled(false);card->setToolTip("Select this marker on the main page. An OFF marker is enabled at the visible peak; click the trace to position it, then use Peak Search as needed.");markerCards[i]=card;markerGrid->addWidget(card,int(i/2),int(i%2));connect(card,&QPushButton::clicked,this,[this,i]{selectMarker(i);if(!plot->markers.points[i].enabled){plot->markers.enable(i,true);plot->notifyMarkers();}});}
        v->addWidget(markerPanel);
        auto *deltaRow=new QHBoxLayout;deltaRow->setContentsMargins(4,0,4,0);deltaRow->setSpacing(4);mainDelta=new QPushButton("Delta OFF");mainReference=new QPushButton("Ref M1");for(auto *b:{mainDelta,mainReference}){b->setFocusPolicy(Qt::NoFocus);b->setFixedSize(82,28);b->setEnabled(false);b->setStyleSheet("font-size:11px;padding:1px");deltaRow->addWidget(b);}marker=new QLabel("M1..M4 values appear above / awaiting data");marker->setObjectName("mainMarkerDelta");marker->setMinimumWidth(0);marker->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);marker->setFixedHeight(28);marker->setStyleSheet("font-size:12px;color:#22e9f1");deltaRow->addWidget(marker,1);v->addLayout(deltaRow);
        mainDelta->setToolTip("Toggle signed differences: active marker minus reference marker.");mainReference->setToolTip("Cycle delta reference M1..M4. OFF/unavailable references produce no numeric difference.");connect(mainDelta,&QPushButton::clicked,this,[this]{plot->markers.deltaEnabled=!plot->markers.deltaEnabled;plot->notifyMarkers();});connect(mainReference,&QPushButton::clicked,this,[this]{plot->markers.reference=(plot->markers.reference+1)%4;plot->notifyMarkers();});
        plot=new Plot;plot->setMinimumSize(300,180);history=new History;history->setMinimumHeight(70);history->setMaximumHeight(100);history->hide();v->addWidget(plot,1);v->addWidget(history);
        auto *stats=new QHBoxLayout;stats->setSpacing(3);auto item=[&](QString title,QLabel *&value){auto *box=new QWidget;auto *layout=new QVBoxLayout(box);layout->setContentsMargins(8,2,8,2);auto *name=new QLabel(title);name->setStyleSheet("color:#a8bac8;font-size:11px");value=new QLabel("--");value->setObjectName("metric");layout->addWidget(name);layout->addWidget(value);stats->addWidget(box);};
        item("BIN SPACING",df);item("WINDOW ENBW",enbw);item("TONE ESTIMATE",tone);item("TOTAL POWER",power);v->addLayout(stats);
        cwInput=new QLabel("CW input power / no RX calibration profile loaded");cwInput->setMargin(4);cwInput->setStyleSheet("font-size:13px;color:#a8bac8");v->addWidget(cwInput);
        plot->markerChanged=[this](double,double){updateMarkerReadout();};
        rxSettings=new QDialog(this);rxSettings->setWindowTitle("SA3000 / acquisition and display");rxSettings->setMinimumWidth(470);auto *layout=new QVBoxLayout(rxSettings);auto *form=new QFormLayout;layout->addLayout(form);
        source=new QComboBox;source->addItems({"Linux RX device","Saved board record"});fft=new QComboBox;for(unsigned n:{1024u,8192u,65536u}){const auto &w=resources.windows.at(n);const double rbw=150e6*double(w.squareSum)/(double(w.sum)*double(w.sum));fft->addItem(QString("%1 Hz / N=%2").arg(rbw,0,'f',2).arg(n),n);}fft->setCurrentIndex(2);
        averages=new QSpinBox;averages->setRange(1,256);averages->setValue(1);averages->setAccessibleName("Linear averages");
        span=new QDoubleSpinBox;span->setRange(.01,1000);span->setValue(150);span->setSuffix(" MHz");span->setAccessibleName("Display span");span->setProperty("frequencyMHz",true);
        ref=new QDoubleSpinBox;ref->setRange(-180,40);ref->setValue(0);ref->setSuffix(" dBFS");ref->setAccessibleName("Display reference");
        yrange=new QDoubleSpinBox;yrange->setRange(20,240);yrange->setValue(120);yrange->setSuffix(" dB");yrange->setAccessibleName("Display range");
        rxCenter=new QDoubleSpinBox;rxCenter->setRange(1300,2300);rxCenter->setDecimals(6);rxCenter->setValue(1800);rxCenter->setSuffix(" MHz");rxCenter->setProperty("frequencyMHz",true);rxCenter->setKeyboardTracking(false);
        form->addRow("Source",source);form->addRow("RBW (Hann ENBW)",fft);form->addRow("Linear averages",averages);form->addRow("RX center",rxCenter);form->addRow("Display span",span);form->addRow("Reference",ref);form->addRow("Y range",yrange);
        sweepMode=new QComboBox;sweepMode->addItems({"Auto / fastest validated sweep","Manual / minimum acquisition dwell"});
        sweepSeconds=new QDoubleSpinBox;sweepSeconds->setRange(.1,60);sweepSeconds->setDecimals(3);sweepSeconds->setValue(1);sweepSeconds->setSuffix(" s");sweepSeconds->setAccessibleName("Sweep time minimum");sweepSeconds->setKeyboardTracking(false);
        sweepTimeNote=new QLabel("Auto; manual acquisition dwell requires live Span >150MHz");sweepTimeNote->setWordWrap(true);
        vbwMode=new QComboBox;vbwMode->addItems({"Auto / existing linear average","Manual / equivalent FFT video average"});
        vbwHz=new QDoubleSpinBox;vbwHz->setRange(.1,1000000);vbwHz->setDecimals(2);vbwHz->setValue(1000);vbwHz->setSuffix(" Hz");vbwHz->setAccessibleName("Equivalent VBW");vbwHz->setKeyboardTracking(false);
        vbwNote=new QLabel;vbwNote->setWordWrap(true);vbwNote->setStyleSheet("font-size:12px;color:#a8bac8");
        connect(vbwMode,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{refreshCapabilities();});connect(vbwHz,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this]{refreshCapabilities();});
        form->addRow("Sweep Time",sweepMode);form->addRow("Requested minimum",sweepSeconds);form->addRow(sweepTimeNote);form->addRow("VBW mode",vbwMode);form->addRow("Equivalent VBW",vbwHz);form->addRow(vbwNote);
        connect(sweepMode,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{refreshCapabilities();});
        hold=new QCheckBox("Maximum hold of displayed frames");layout->addWidget(hold);directory=button(layout,"SELECT OUTPUT DIRECTORY",[this]{selectOutput();});button(layout,"CLOSE",[this]{rxSettings->hide();});
        connect(span,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this](double width){rxCenter->setRange(width>150?1300+width/2:1300,width>150?2300-width/2:2300);});
        connect(rxCenter,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this]{clearHistory();refreshCapabilities();});
        connect(hold,&QCheckBox::toggled,this,[this](bool on){plot->holdOn=on;plot->clearHold();});
        connect(source,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{repeatGate.stop();replay.clear();averages->setValue(1);clearHistory();badge->setText(source->currentIndex()==0?"LOCAL DEVICE MODE / awaiting capture":"REPLAY / saved board data");refreshCapabilities();});
        connect(fft,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{repeatGate.stop();replay.clear();averages->setValue(1);clearHistory();refreshCapabilities();});
        for(auto *control:{span,ref,yrange})connect(control,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this]{plot->span=span->value()*1e6;plot->reference=ref->value();plot->range=yrange->value();plot->viewChanged();refreshCapabilities();});
        return page;
    }
    void saveSpectrumUi(const fs::path &directory,unsigned framesMin,unsigned framesMax,double rbw){
        try{QJsonObject o;o["kind"]="SPECTRUM_UI_MEASUREMENT_SETTINGS";o["vbw_mode"]=vbwMode->currentIndex()==1&&source->currentIndex()==0?"EQUIVALENT_FFT_VIDEO_AVERAGE":"AUTO_EXISTING_LINEAR_AVERAGE";o["requested_equivalent_vbw_hz"]=vbwMode->currentIndex()==1&&source->currentIndex()==0?vbwHz->value():0;o["rbw_basis"]="Hann window ENBW, not3dB RBW";o["rbw_enbw_hz"]=rbw;o["minimum_linear_averages"]=averages->value();o["effective_frames_min"]=int(framesMin);o["effective_frames_max"]=int(framesMax);o["estimated_equivalent_vbw_min_hz"]=equivalentVideoBandwidth(rbw,framesMax);o["estimated_equivalent_vbw_max_hz"]=equivalentVideoBandwidth(rbw,framesMin);o["continuous_time_3db_vbw_certified"]=false;o["marker_units"]="dBFS/bin; differences in Hz and dB";o["reference_marker"]=int(plot->markers.reference+1);o["delta_enabled"]=plot->markers.deltaEnabled;QJsonArray markers;
            for(unsigned i=0;i<4;++i){const auto &m=plot->markers.points[i];QJsonObject item;item["marker"]=int(i+1);item["enabled"]=m.enabled;item["valid"]=m.valid;item["tracking"]=m.tracking;item["frequency_hz"]=m.valid?QJsonValue(m.frequency):QJsonValue(QJsonValue::Null);item["level_dbfs_per_bin"]=m.valid?QJsonValue(m.level):QJsonValue(QJsonValue::Null);markers.append(item);}o["markers"]=markers;const auto bytes=QJsonDocument(o).toJson(QJsonDocument::Indented);writeFile(directory/"video_marker_settings.json",Bytes(bytes.begin(),bytes.end()));
        }catch(const std::exception &e){message("Valid raw spectrum retained; UI settings export failed: "+errorText(e));}
    }
    VideoPlan videoPlan()const{const unsigned n=fft->currentData().toUInt();const auto &w=resources.windows.at(n);const double rbw=150e6*double(w.squareSum)/(double(w.sum)*double(w.sum));return planVideoAverages(rbw,unsigned(averages->value()),source->currentIndex()==0&&vbwMode->currentIndex()==1?vbwHz->value():0);}
    unsigned acquisitionFrames()const{return videoPlan().frames;}
    void updateMarkerReadout(){
        if(!marker||!plot->hasData)return;
        auto &bank=plot->markers;const QStringList colors={"#52e1cf","#ff82dc","#72b7ff","#ffb34b"};
        for(unsigned i=0;i<4;++i){const auto &m=bank.points[i];auto *card=markerCards[i];card->setEnabled(true);const QString state=!m.enabled?"OFF":m.tracking?"PEAK TRACK":"FIXED";
            card->setText(QString("M%1%2 / %3\n%4").arg(i+1).arg(i==bank.active?" *":"").arg(state).arg(!m.enabled?"-- MHz / -- dBFS/bin":!m.valid?"Outside acquired data":QString("%1 MHz / %2 dBFS/bin").arg(m.frequency/1e6,0,'f',6).arg(m.level,0,'f',2)));
            const QString css=QString("QPushButton{color:%1;background:%2;border:%3px solid %4;border-radius:4px;padding:1px;font-size:14px;text-align:left}QPushButton:hover{background:#263c4b;}").arg(m.enabled?colors[int(i)]:"#82929d",i==bank.active?"#203849":"#13232e").arg(i==bank.active?2:1).arg(i==bank.active?colors[int(i)]:"#3a4b56");if(card->styleSheet()!=css)card->setStyleSheet(css);
        }
        mainDelta->setEnabled(true);mainReference->setEnabled(true);mainDelta->setText(bank.deltaEnabled?"Delta ON":"Delta OFF");mainReference->setText(QString("Ref M%1").arg(bank.reference+1));
        QString readout=QString("Active M%1 / click card then trace to position").arg(bank.active+1);
        if(bank.deltaEnabled){const auto d=bank.delta(bank.active);readout=QString("M%1-M%2: ").arg(bank.active+1).arg(bank.reference+1)+(d.valid?QString("df=%1 kHz / dL=%2 dB").arg(d.frequency/1000,0,'f',3).arg(d.level,0,'f',3):"reference/active unavailable");}
        marker->setText(readout);marker->setToolTip("Current yellow trace / coherent dBFS per FFT bin. Delta dL is dB, not dBm or watts. Swept segments are sequential.");
        if(!markerDialog)return;
        const QSignalBlocker b1(activeMarker),b2(referenceMarker),b3(markerEnable),b4(markerTrack),b5(deltaMode),b6(markerFrequency),b7(markerVisible);
        activeMarker->setCurrentIndex(int(bank.active));referenceMarker->setCurrentIndex(int(bank.reference));const auto &m=bank.points[bank.active];markerEnable->setChecked(m.enabled);markerTrack->setChecked(m.tracking);deltaMode->setChecked(bank.deltaEnabled);markerVisible->setChecked(plot->markerOn);
        const auto *editor=markerFrequency->findChild<QLineEdit*>();const bool editing=markerFrequency->hasFocus()||(editor&&(editor->hasFocus()||editor->isModified()));if(!editing){markerFrequency->setValue(m.positioned?m.frequency/1e6:plot->data.center/1e6);}markerFrequency->setEnabled(m.enabled&&!m.tracking);markerTrack->setEnabled(m.enabled);
        for(unsigned i=0;i<4;++i){const auto &v=bank.points[i];const auto d=bank.delta(i);const QStringList row={QString("M%1%2").arg(i+1).arg(v.enabled?"":" OFF"),v.enabled&&v.valid?QString::number(v.frequency/1e6,'f',6):"--",v.enabled&&v.valid?QString::number(v.level,'f',3):"--",v.tracking?"Peak track":"Fixed",bank.deltaEnabled&&d.valid?QString::number(d.frequency/1000,'f',3):"--",bank.deltaEnabled&&d.valid?QString::number(d.level,'f',3):"--"};for(unsigned col=0;col<6;++col)markerTable->item(int(i),int(col))->setText(row[int(col)]);}
        markerTable->selectRow(int(bank.active));
    }
    void selectMarker(unsigned id){if(markerFrequency)markerFrequency->interpretText();plot->markers.active=id;plot->notifyMarkers();}
    void showMarkerSettings(){
        if(!plot->hasData)return;
        if(!markerDialog){markerDialog=new QDialog(this);markerDialog->setWindowTitle("SA3000 / M1..M4 / delta markers");markerDialog->setMinimumSize(850,480);auto *layout=new QVBoxLayout(markerDialog);auto *form=new QFormLayout;layout->addLayout(form);
            activeMarker=new QComboBox;referenceMarker=new QComboBox;for(const auto &name:{"M1","M2","M3","M4"}){activeMarker->addItem(name);referenceMarker->addItem(name);}
            markerEnable=new QCheckBox("Enable selected marker");markerTrack=new QCheckBox("Track strongest peak inside displayed span");deltaMode=new QCheckBox("Show signed delta against reference marker");markerVisible=new QCheckBox("Show marker lines");
            markerFrequency=new QDoubleSpinBox;markerFrequency->setRange(1300,2300);markerFrequency->setDecimals(6);markerFrequency->setSuffix(" MHz");markerFrequency->setKeyboardTracking(false);markerFrequency->setAccessibleName("Marker frequency");markerFrequency->setProperty("frequencyMHz",true);
            form->addRow("Active marker",activeMarker);form->addRow(markerEnable);form->addRow("Frequency / nearest FFT bin",markerFrequency);form->addRow(markerTrack);form->addRow("Delta reference",referenceMarker);form->addRow(deltaMode);form->addRow(markerVisible);
            markerTable=new QTableWidget(4,6);markerTable->setHorizontalHeaderLabels({"Marker","RF MHz","dBFS/bin","Mode","Delta kHz","Delta dB"});markerTable->verticalHeader()->hide();markerTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);markerTable->setSelectionBehavior(QAbstractItemView::SelectRows);markerTable->setEditTriggers(QAbstractItemView::NoEditTriggers);markerTable->setStyleSheet("QHeaderView::section{background:#344651;color:#e8f0f7;padding:4px} QTableWidget{gridline-color:#394750}");for(int row=0;row<4;++row)for(int col=0;col<6;++col)markerTable->setItem(row,col,new QTableWidgetItem);layout->addWidget(markerTable);
            auto *note=new QLabel("Markers read the current yellow trace, snapped to real FFT RF bins. Click the plot to position the active marker. Delta f = active - reference; Delta level is dB. Swept bands are sequential and have no full-band dBm calibration.");note->setWordWrap(true);layout->addWidget(note);
            auto *buttons=new QHBoxLayout;layout->addLayout(buttons);auto *peak=new QPushButton("PEAK SEARCH / ACTIVE");auto *four=new QPushButton("FIND UP TO 4 LOCAL MAXIMA");auto *close=new QPushButton("CLOSE");for(auto *b:{peak,four,close})buttons->addWidget(b);
            connect(activeMarker,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int id){selectMarker(unsigned(id));});connect(referenceMarker,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int id){plot->markers.reference=unsigned(id);plot->notifyMarkers();});
            connect(markerEnable,&QCheckBox::toggled,this,[this](bool on){plot->markers.enable(plot->markers.active,on);plot->notifyMarkers();});connect(markerTrack,&QCheckBox::toggled,this,[this](bool on){plot->markers.track(plot->markers.active,on);plot->notifyMarkers();});connect(deltaMode,&QCheckBox::toggled,this,[this](bool on){plot->markers.deltaEnabled=on;plot->notifyMarkers();});connect(markerVisible,&QCheckBox::toggled,this,[this](bool on){plot->markerOn=on;plot->update();});
            connect(markerFrequency,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this](double hz){plot->markers.position(plot->markers.active,hz*1e6);plot->notifyMarkers();});connect(markerTable,&QTableWidget::cellClicked,this,[this](int row,int){activeMarker->setCurrentIndex(row);});connect(peak,&QPushButton::clicked,this,[this]{plot->markerOn=true;plot->peak();});connect(four,&QPushButton::clicked,this,[this]{plot->markerOn=true;plot->fourPeaks();});connect(close,&QPushButton::clicked,markerDialog,&QDialog::hide);
        }
        updateMarkerReadout();markerDialog->show();markerDialog->raise();
    }
    QWidget *txPage(){
        auto *page=new QWidget;auto *v=new QVBoxLayout(page);v->setContentsMargins(18,12,18,10);v->setSpacing(8);
        auto *title=new QLabel("GENERATOR / NATIVE PS WAVEFORM CONTROL");title->setStyleSheet("font-size:18px;font-weight:bold");v->addWidget(title);
        auto *notice=new QLabel("Generate IQ > verified DDR4 upload > explicit RF PLAY. Edits require a new generation/upload.");notice->setWordWrap(true);notice->setStyleSheet("color:#a8bac8;font-size:12px");v->addWidget(notice);
        txParameters=new QWidget;auto *columns=new QHBoxLayout(txParameters);columns->setContentsMargins(0,0,0,0);auto group=[&](QString title){auto *box=new QGroupBox(title);auto *form=new QFormLayout(box);form->setVerticalSpacing(10);columns->addWidget(box);return form;};auto *rf=group("RF / WAVEFORM");auto *mod=group("MODULATION");v->addWidget(txParameters,1);
        waveType=new QComboBox;waveType->addItems({"CW","PULSE","LFM","LFM_PULSE","BARKER13","PSK","QAM"});order=new QComboBox;
        auto number=[](double lo,double hi,double initial,QString suffix,int precision,QString name){auto *x=new QDoubleSpinBox;x->setRange(lo,hi);x->setDecimals(precision);x->setValue(initial);x->setSuffix(suffix);x->setAccessibleName(name);x->setKeyboardTracking(false);return x;};
        rfFrequency=number(1300,2300,1790," MHz",6,"RF frequency");offset=number(-500,500,10," MHz",6,"IQ offset");rfFrequency->setProperty("frequencyMHz",true);offset->setProperty("frequencyMHz",true);
        peakCounts=new QSpinBox;peakCounts->setRange(1,32700);peakCounts->setValue(2048);peakCounts->setAccessibleName("Digital peak counts");peakCounts->setKeyboardTracking(false);
        try{txCalibration=loadTxCwCalibration(text(resourcesPath/"tx_cw_calibration.json"),text(resourcesPath/"tx_cw_verification.json"));}catch(const std::exception &e){txCalibrationError=errorText(e);}
        try{if(!txCalibration)throw std::runtime_error("Original CW model unavailable");txContextCalibration=loadTxCw4025gContext(*txCalibration,text(resourcesPath/"tx_cw_4025g_context.json"),text(resourcesPath/"tx_cw_4025g_verification.json"));}catch(const std::exception &e){txContextError=errorText(e);}
        powerContext=new QComboBox;powerContext->addItems({"FSVA / original cable","4025G / current cable"});powerContext->setCurrentIndex(txContextCalibration?1:0);
        powerMode=new QComboBox;powerMode->addItems({"CW model","Digital / UNCALIBRATED"});powerUnit=new QComboBox;powerUnit->addItems({"dBm","dBW"});
        targetPower=number(txContextCalibration?-40:-50,txContextCalibration?-20:-10,-25," dBm",2,"CW output power");targetPower->setSingleStep(1);
        txAdvanced=new QDialog(this);txAdvanced->setWindowTitle("Generator / advanced digital amplitude");auto *advancedLayout=new QFormLayout(txAdvanced);advancedLayout->addRow("Digital component peak",peakCounts);
        auto *digitalNote=new QLabel("Used only in Digital / UNCALIBRATED mode. Counts are not dBm.\nChanging amplitude requires a fresh generation and upload.");digitalNote->setWordWrap(true);advancedLayout->addRow(digitalNote);
        auto *advancedClose=new QPushButton("CLOSE");advancedLayout->addRow(advancedClose);connect(advancedClose,&QPushButton::clicked,txAdvanced,&QDialog::hide);
        pulseWidth=number(.001,10000,10," us",3,"Pulse width");pulsePeriod=number(1,10000,100," us",3,"Pulse period");chirpWidth=number(.001,40,20," MHz",3,"LFM bandwidth");chirpWidth->setProperty("frequencyMHz",true);
        symbolRate=number(.1,10,5," Msym/s",6,"Symbol rate");rolloff=number(.01,1,.35,"",2,"RRC rolloff");
        rf->addRow("Waveform",waveType);rf->addRow("RF frequency",rfFrequency);rf->addRow("IQ offset",offset);rf->addRow("Power mode",powerMode);rf->addRow("Reference",powerContext);rf->addRow("Target level",targetPower);rf->addRow("Unit",powerUnit);rf->addRow("Record",new QLabel("10 ms / 48 MB"));rf->addRow("Complex rate",new QLabel("1.2 GS/s"));powerNote=new QLabel;powerNote->setWordWrap(true);powerNote->setStyleSheet("font-size:11px;color:#a8bac8");rf->addRow(powerNote);
        mod->addRow("Order",order);mod->addRow("Symbol rate",symbolRate);mod->addRow("RRC rolloff",rolloff);mod->addRow("Pulse width",pulseWidth);mod->addRow("Pulse period",pulsePeriod);mod->addRow("LFM bandwidth",chirpWidth);mod->addRow(new QLabel("Barker13: 13 chips x 1 us"));
        connect(offset,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this](double value){QSignalBlocker b(rfFrequency);rfFrequency->setValue(1800-value);generatorEdited();});connect(rfFrequency,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this](double value){QSignalBlocker b(offset);offset->setValue(1800-value);generatorEdited();});
        auto typeChanged=[this]{auto type=waveType->currentText();QSignalBlocker blocked(order);order->clear();order->addItems(type=="QAM"?QStringList{"16","64","256"}:QStringList{"2","4","8"});order->setEnabled(type=="QAM"||type=="PSK");if(type=="PSK")order->setCurrentText("4");if(type!="CW")powerMode->setCurrentIndex(1);offset->setValue(type=="CW"?10:0);pulseWidth->setEnabled(type=="PULSE"||type=="LFM_PULSE");pulsePeriod->setEnabled(type=="PULSE"||type=="LFM"||type=="LFM_PULSE"||type=="BARKER13");chirpWidth->setEnabled(type=="LFM"||type=="LFM_PULSE");symbolRate->setEnabled(type=="PSK"||type=="QAM");rolloff->setEnabled(type=="PSK"||type=="QAM");generatorEdited();};connect(waveType,qOverload<int>(&QComboBox::currentIndexChanged),this,[typeChanged]{typeChanged();});typeChanged();
        waveFile=new QLabel("No waveform selected. Generate a new record or select an existing IQ + JSON.");waveFile->setWordWrap(true);waveFile->setStyleSheet("font-size:12px;color:#38c2ff");v->addWidget(waveFile);
        auto *actions=new QHBoxLayout;generate=new QPushButton("GENERATE IQ");txUpload=new QPushButton("UPLOAD TO DDR4");txPlay=new QPushButton("RF PLAY");txStop=new QPushButton("RF STOP");
        for(auto *b:{generate,txUpload,txPlay,txStop}){actions->addWidget(b);saButtonStyle(b,b==generate?"blue":b==txPlay?"yellow":"soft",1,13);b->setMinimumHeight(42);}v->addLayout(actions);
        connect(generate,&QPushButton::clicked,this,[this]{makeWave();});connect(txUpload,&QPushButton::clicked,this,[this]{uploadTx();});connect(txPlay,&QPushButton::clicked,this,[this]{playTx();});connect(txStop,&QPushButton::clicked,this,[this]{stopTx();});
        auto *files=new QHBoxLayout;selectIq=new QPushButton("SELECT EXISTING IQ");auto *cal=new QPushButton("POWER MODEL / SCOPE");auto *advanced=new QPushButton("ADVANCED COUNTS");files->addWidget(selectIq);files->addWidget(cal);files->addWidget(advanced);connect(advanced,&QPushButton::clicked,this,[this]{if(!busy)txAdvanced->show();});v->addLayout(files);connect(selectIq,&QPushButton::clicked,this,[this]{selectWaveFile();});connect(cal,&QPushButton::clicked,this,[this]{showCalibration();});
        txHealth=new QLabel("TX state will be read from the Linux driver; no automatic playback.");txHealth->setWordWrap(true);txHealth->setStyleSheet("font-size:12px;color:#a8bac8");v->addWidget(txHealth);
        connect(powerUnit,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int unit){QSignalBlocker block(targetPower);const double dbm=unit==1?targetPower->value():dbmFromDbw(targetPower->value());setPowerRange(dbm);refreshCapabilities();});
        connect(powerMode,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{generatorEdited();});
        connect(powerContext,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{const double dbm=requestedDbm();setPowerRange(dbm);generatorEdited();});
        connect(targetPower,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this]{generatorEdited();});
        connect(peakCounts,qOverload<int>(&QSpinBox::valueChanged),this,[this]{if(powerMode->currentIndex()==1)generatorEdited();});
        connect(order,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{generatorEdited();});
        for(auto *entry:{pulseWidth,pulsePeriod,chirpWidth,symbolRate,rolloff})connect(entry,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this]{generatorEdited();});
        return page;
    }
    double requestedDbm()const{return powerUnit->currentIndex()==1?dbmFromDbw(targetPower->value()):targetPower->value();}
    const std::optional<TxCwCalibration> &selectedPowerModel()const{return powerContext->currentIndex()==1?txContextCalibration:txCalibration;}
    void setPowerRange(double dbm){const auto &model=selectedPowerModel();const double lo=model?model->minimumDbm:-50,hi=model?model->maximumDbm:-10;const bool watts=powerUnit->currentIndex()==1;QSignalBlocker block(targetPower);targetPower->setRange(lo-(watts?30:0),hi-(watts?30:0));targetPower->setSuffix(watts?" dBW":" dBm");targetPower->setValue(std::clamp(dbm,lo,hi)-(watts?30:0));}
    bool modelPowerReady()const{return selectedPowerModel()&&waveType->currentText()=="CW"&&rfFrequency->value()==1790;}
    void focusPower(){if(powerMode->currentIndex()==0)targetPower->setFocus();else if(!busy){txAdvanced->show();peakCounts->setFocus();}}
    void generatorEdited(){
        if(!waveFile)return;
        const bool previous=txValidatedForControls||!lastWaveform.isEmpty();
        txValidatedForControls=false;lastWaveform.clear();
        if(previous){waveFile->setText("Parameters changed: GENERATE IQ and UPLOAD again before RF PLAY.");if(txFlags&4)stopTx();}
        updateTxButtons();
    }
    void showCalibration(){
        QString note="CW1790MHz only. Select Reference to choose the applicable instrument/cable setup.\n4025G/current cable: desired -40..-20dBm; original model command=desired+0.06dB. One actual corrected -25dBm point gave -25.006667dBm mean,0.01dB reading span. Full range has not been physically retested after correction.\nFSVA/original cable: frozen original model,-50..-10dBm; no4025G correction.\nReference plane is analyzer input including the respective cable, not de-embedded J7 power. Absolute accuracy below0.1dB is not certified. Other waveforms/frequencies remain uncalibrated.\n";
        const auto &model=selectedPowerModel();if(model){note+="Base model SHA256: "+QString::fromStdString(model->profileSha)+"\nSetup: "+QString::fromStdString(model->setupId);if(!model->contextSha.empty())note+="\nContext SHA256: "+QString::fromStdString(model->contextSha);}
        else note+="Unavailable: "+(powerContext->currentIndex()==1?txContextError:txCalibrationError);
        QMessageBox::information(this,"CW power reference / applicability",note);
    }
    void loadRxCalibration(const QString &filename){
        try{auto candidate=loadRxCwCalibration(filename,resources);rxCalibration=std::move(candidate);rxCalibrationFile=filename;rxCalibrationError.clear();
            message("RX CW calibration loaded: "+QString::fromStdString(rxCalibration->id)+"; 1790MHz only, engineering reference");}
        catch(const std::exception&e){rxCalibration.reset();rxCalibrationFile=filename;rxCalibrationError=errorText(e);}
        updateCwReadout();
    }
    void updateCwReadout(){
        if(!cwInput)return;
        cwInput->setStyleSheet("font-size:13px;color:#a8bac8");
        if(!rxCalibrationEnabled){cwInput->setText("CW input power calibration OFF / spectrum remains dBFS");return;}
        if(!rxCalibration){cwInput->setText("CW input power: no validated RX calibration profile");return;}
        if(!plot->hasData){cwInput->setText("CW input power @1790MHz: awaiting live ADC data");return;}
        if(plot->swept){cwInput->setText("Swept spectrum: no full-band dBm calibration");return;}
        if(!plot->live){cwInput->setText("CW input power: calibration is not applied to replay records");return;}
        const auto reading=rxCwPower(plot->data,*rxCalibration);
        if(!reading.valid){cwInput->setText("CW @1790MHz: "+QString::fromStdString(reading.reason));return;}
        cwInput->setStyleSheet("font-size:14px;font-weight:bold;color:#38c2ff");
        cwInput->setText(QString("CW INPUT (J1)  %1 dBm  / 1790MHz CAL / engineering reference").arg(reading.dbm,0,'f',3));
    }
    void saveCwSidecar(const CaptureResult &result){
        if(!result.live||!result.hasData||!rxCalibrationEnabled||!rxCalibration)return;
        const auto &c=*rxCalibration;const auto reading=rxCwPower(result.measurement,c);
        QJsonObject o;o["kind"]="CW_INPUT_POWER_ESTIMATE";o["qualified"]=reading.valid;o["frequency_hz"]=c.frequency;
        o["input_dbm"]=reading.valid?QJsonValue(reading.dbm):QJsonValue(QJsonValue::Null);
        o["reason"]=QString::fromStdString(reading.reason);o["profile_id"]=QString::fromStdString(c.id);o["profile_sha256"]=QString::fromStdString(c.sha256);
        o["gain_db_per_db"]=c.gainDbPerDb;o["offset_db"]=c.offsetDb;o["formula"]="input_dbm = gain_db_per_db * cw_dbfs + offset_db";o["estimator"]="energy_5_bin_noise_subtracted";o["reference_plane"]="XM500_J1_INPUT";o["absolute_accuracy_claimed_db"]=QJsonValue(QJsonValue::Null);
        auto bytes=QJsonDocument(o).toJson(QJsonDocument::Indented);writeFile(result.output/"measurement/cw_input_power.json",Bytes(bytes.begin(),bytes.end()));
    }
    void showRxPowerCalibration(){
        auto *dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowTitle("SA3000 / receiver CW input calibration");dialog->setMinimumWidth(620);
        auto *layout=new QVBoxLayout(dialog);auto *status=new QLabel;status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);
        auto *enabled=new QCheckBox("Enable qualified CW input power readout");enabled->setChecked(rxCalibrationEnabled);layout->addWidget(enabled);
        auto refresh=[this,status]{if(!rxCalibration)status->setText("No qualified calibration loaded.\n"+rxCalibrationError+"\n"+rxCalibrationFile);
            else {const auto &c=*rxCalibration;status->setText(QString("%1\nReference: %2 / XM500 J1\nFrequency: 1790MHz / FFT65536 / RX150MS/s / average >=16\nInput range: %3 to %4 dBm\nIntercept: %5 dB / local SNR >= %6 dB\nScale: %11 dB/dB; input_dBm = scale * CW_dBFS + intercept\nTrain max residual: %7 dB / independent max error: %8 dB\nProfile SHA256: %9\nFile: %10").arg(QString::fromStdString(c.id),QString::fromStdString(c.reference)).arg(c.minimumDbm,0,'f',3).arg(c.maximumDbm,0,'f',3).arg(c.offsetDb,0,'f',6).arg(c.minSnrDb).arg(c.trainResidualDb,0,'f',4).arg(c.holdoutResidualDb,0,'f',4).arg(QString::fromStdString(c.sha256),rxCalibrationFile).arg(c.gainDbPerDb,0,'f',9));}};
        connect(enabled,&QCheckBox::toggled,this,[this](bool on){rxCalibrationEnabled=on;updateCwReadout();});
        button(layout,"RELOAD PROFILE",[this,refresh]{loadRxCalibration(rxCalibrationFile);refresh();});
        button(layout,"IMPORT QUALIFIED JSON",[this,refresh]{auto file=QFileDialog::getOpenFileName(this,"Select measured RX CW calibration",outputPath,"Calibration (*.json)");if(!file.isEmpty()){loadRxCalibration(file);refresh();}});
        auto *note=new QLabel("CW input estimate only. The spectrum and TOTAL POWER retain dBFS. Not a channel/noise/QAM power calibration; not a guarantee of absolute accuracy below0.1dB. Profiles must include independent validation. No RF or ADC setting is changed.");note->setWordWrap(true);layout->addWidget(note);button(layout,"CLOSE",[dialog]{dialog->close();});refresh();dialog->show();
    }
    void refreshCapabilities(){
        if(!canvas||nav.empty()||!single||!txPlay)return;
        const bool rx=source->currentIndex()!=0||rxAvailable(),data=plot->hasData,gen=currentPage==1,spectrum=currentPage==0;
        multiView->setEnabled(spectrum);
        bool videoReady=true;QString videoSummary;
        try{const auto v=videoPlan();videoSummary=QString("avg%1 / EqVBW%2Hz").arg(v.frames).arg(v.equivalentHz,0,'f',1);vbwNote->setText(QString("FFT video-average equivalence: %1 frames, estimated %2 Hz. Not a continuous-time3dB video filter.").arg(v.frames).arg(v.equivalentHz,0,'f',2));}catch(const std::exception &e){videoReady=false;videoSummary="VBW unavailable";vbwNote->setText(errorText(e));}
        vbwMode->setEnabled(!busy&&source->currentIndex()==0);vbwHz->setEnabled(!busy&&source->currentIndex()==0&&vbwMode->currentIndex()==1);
        single->setEnabled(spectrum&&!busy&&rx&&!liveFault&&videoReady);repeat->setEnabled(spectrum&&!busy&&rx&&!liveFault&&videoReady);
        generate->setEnabled(!busy&&(powerMode->currentIndex()==1||modelPowerReady()));selectIq->setEnabled(!busy);open->setEnabled(!busy);
        const bool calibratedMode=powerMode->currentIndex()==0;
        targetPower->setEnabled(!busy&&calibratedMode);powerUnit->setEnabled(!busy&&calibratedMode);powerContext->setEnabled(!busy&&calibratedMode);peakCounts->setEnabled(!busy&&!calibratedMode);
        if(powerNote){powerNote->setText(calibratedMode?(modelPowerReady()?(powerContext->currentIndex()==1?"4025G + CURRENT CABLE / CW1790MHz / -40..-20dBm. +0.06dB applied; independent -25dBm spot check passed. Not absolute accuracy certification.":"ORIGINAL FSVA + CABLE / CW1790MHz / -50..-10dBm. Current4025G baseline includes a0.113dB deviation; no context correction."):"Power model unavailable at this waveform/frequency. Select Digital mode or CW1790MHz."):QString("UNCALIBRATED digital mode / peak %1 counts. RF=1800-IQ offset.").arg(peakCounts->value()));}
        const bool sweptLive=source->currentIndex()==0&&span->value()>150;
        sweepMode->setEnabled(!busy&&sweptLive);sweepSeconds->setEnabled(!busy&&sweptLive&&sweepMode->currentIndex()==1);
        const QStringList rxLabels={"Source",QString("RBW\n%1 kHz").arg(fft->currentText().section(' ',0,0).toDouble()/1000,0,'f',2),QString("Average\n%1").arg(averages->value()),QString("Span\n%1 MHz").arg(span->value()),QString("Ref Level\n%1 dBFS").arg(ref->value()),"RX STOP"};
        const QStringList txLabels={waveType->currentText(),QString("RF\n%1 MHz").arg(rfFrequency->value(),0,'f',3),(calibratedMode?QString("Level\n%1 %2").arg(targetPower->value(),0,'f',2).arg(powerUnit->currentText()):QString("Peak\n%1 counts").arg(peakCounts->value())),"Modulation","Record\n10 ms","RF STOP"};
        for(unsigned k=0;k<6;++k){softKeys[k]->setText(gen?txLabels[int(k)]:rxLabels[int(k)]);softKeys[k]->setEnabled(gen?(k==5?bool(tx)&&!txStopTask.joinable()&&((txFlags&4)||txBusy):k<4&&!busy&&(k!=3||order->isEnabled())):spectrum&&(k==5?receiving:k<5&&!busy));}
        for(unsigned k=0;k<functionKeys.size();++k){const bool implemented=k==0||k==1||k==2||k==3||k==4||k==5||k==6||k==8||k==9||k==10||k==11;
            bool enabled=implemented&&(spectrum||gen);if(k==0)enabled=(gen||spectrum)&&!busy;else if(gen)enabled=(k==2||k==11)&&!busy;else if(k==3||k==8)enabled=spectrum&&data;else if(k==10)enabled=spectrum&&!busy&&sweptLive;else if(k!=6&&k!=9)enabled=enabled&&!busy;functionKeys[k]->setEnabled(enabled);}
        for(const auto &pos:positioned)if(auto *b=qobject_cast<QPushButton*>(pos.widget))if(b->property("needsTrace").toBool())b->setEnabled(spectrum&&data);
        if(connection)connection->setText(QString("RX %1 / TX %2").arg(rxAvailable()?"ready":"offline").arg(txAvailable()?"ready":"offline"));
        if(rxState)rxState->setText(streaming?"RX STREAM":receiving?"RX CAPTURE":"RX STOPPED");
        if(meta)meta->setText(QString("Ref %1 dBFS / %2 dB range       Requested center %5 MHz\nFFT %3 / minimum avg %4 / %6 / Hann").arg(ref->value()).arg(yrange->value()).arg(fft->currentData().toUInt()).arg(averages->value()).arg(rxCenter->value(),0,'f',3).arg(videoSummary));
        updateEntry();updateCwReadout();updateMarkerReadout();
    }
    QWidget *systemPage(){auto *p=new QWidget;auto *v=new QVBoxLayout(p);health=new QPlainTextEdit;health->setReadOnly(true);v->addWidget(health);button(v,"REFRESH LOCAL STATUS",[this]{refreshHealth();});log=new QPlainTextEdit;log->setReadOnly(true);log->document()->setMaximumBlockCount(1500);v->addWidget(log,2);return p;}
    void refreshHealth(){updateTxButtons();health->setPlainText(QString("Executable: native C++ / Qt %1\nLocal receiver: %2\nLinux TX interface: /dev/zcu111_tx\nLinux TX board check: 10ms upload/readback and 1s play/stop passed\nRF frequency, modulation quality and output power still need receiver/instrument measurement\nRFDC/clock readiness: must be initialized by the board service\nRX tuning1300..2300MHz / zoom0 / ADC2.4GS/s / RX150MS/s; wide spans use sequential scans\nStorage: %3\n\nNo Python, PYNQ, JTAG control, or host PC is required by this application.\nThe GUI never maps /dev/mem or uses bare-metal PS DDR addresses.").arg(qVersion()).arg(rxAvailable()?"character device readable":"unavailable").arg(outputPath));}
    void message(const QString &s){auto utf8=s.toUtf8();fprintf(stdout,"INSTRUMENT_EVENT %s\n",utf8.constData());fflush(stdout);if(log)log->appendPlainText(QDateTime::currentDateTime().toString("HH:mm:ss  ")+s);if(eventLine){eventLine->setText(eventLine->fontMetrics().elidedText(s,Qt::ElideRight,eventLine->width()));eventLine->setToolTip(s);}statusBar()->showMessage(s);}
    void fail(const QString &s){lastError=s;badge->setText("ERROR / "+s);message(s);if(autoTest)QCoreApplication::exit(2);}
    void clearHistory(){plot->clearHold();history->clear();}
    void completeUi(){receiving=false;txBusy=false;setBusy(false);if(closing)QTimer::singleShot(0,this,[this]{close();});}
    void launchTask(std::function<void()> action){try{task=std::thread(std::move(action));}catch(const std::exception &e){repeatGate.stop();completeUi();fail("Cannot start worker: "+errorText(e));}}
    void setBusy(bool value){busy=value;for(auto *b:{single,repeat,open,generate,directory,selectIq})b->setEnabled(!value);source->setEnabled(!value);rxCenter->setEnabled(!value);span->setEnabled(!value);fft->setEnabled(!value);averages->setEnabled(!value);txParameters->setEnabled(!value);txAdvanced->setEnabled(!value);stop->setEnabled(value||repeatGate.enabled());updateTxButtons();refreshCapabilities();}
    fs::path freshOutput(const QString &prefix){if(!QDir().mkpath(outputPath))throw std::runtime_error("Cannot create storage directory");return path(outputPath+"/"+prefix+"_"+QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_")+QUuid::createUuid().toString(QUuid::WithoutBraces));}
    void selectReplay(){repeatGate.stop();stop->setEnabled(false);auto files=QFileDialog::getOpenFileNames(this,"Select independent packet records; .receipt.json required",text(resourcesPath/"fixtures"),"Spectrum (*.bin)");if(files.isEmpty())return;try{if(files.size()>256)throw std::runtime_error("At most 256 frames");const auto r=loadReceipt(path(files.front()+".receipt.json"));if(r.logn>16)throw std::runtime_error("Invalid FFT size");const auto n=1u<<r.logn;if(!resources.windows.count(n))throw std::runtime_error("Unsupported FFT size");source->setCurrentIndex(1);fft->setCurrentIndex(fft->findData(n));replay.clear();for(const auto &file:files)replay.push_back(path(file));averages->setValue(files.size());message("Replay packets selected; data will be strictly validated.");}catch(const std::exception &e){fail(errorText(e));}}
    void startSweep(bool repeating){
        if(busy)return;
        try{
            if(source->currentIndex()!=0||!rxAvailable()||liveFault)throw std::runtime_error("Sweep requires a ready Linux RX device");
            SweepRequest request;request.start=(rxCenter->value()-span->value()/2)*1e6;request.stop=(rxCenter->value()+span->value()/2)*1e6;
            request.n=fft->currentData().toUInt();request.frames=acquisitionFrames();request.archive=!repeating;request.targetSeconds=sweepMode->currentIndex()==1?sweepSeconds->value():0;
            planSweep(request.start,request.stop);
            if(request.archive)request.output=freshOutput("sweep");
            const auto token=repeatGate.begin(repeating);cancelled.store(false);receiving=true;setBusy(true);progress->setValue(0);lastError.clear();
            message("Sequential NCO sweep started; complete rounds only; no automatic RF output.");
            if(task.joinable())task.join();
            launchTask([this,request,token]{try{
                auto result=sweepSpectra(request,resources,cancelled,[this](unsigned i,unsigned n){QMetaObject::invokeMethod(this,[this,i,n]{progress->setValue(int(100.*i/n));},Qt::QueuedConnection);});
                QMetaObject::invokeMethod(this,[this,request,token,result=std::move(result)]()mutable{
                    if(task.joinable()){task.join();}completeUi();
                    if(result.complete){
                        ++sweepShown;plot->setSweep(result);if(request.archive)saveSpectrumUi(request.output,*std::min_element(result.effectiveFrames.begin(),result.effectiveFrames.end()),*std::max_element(result.effectiveFrames.begin(),result.effectiveFrames.end()),result.enbw);
                        sweepTimeNote->setText(QString("%1 / actual %2s / effective averages %3..%4 per segment").arg(result.targetSeconds>0?QString("Requested minimum %1s").arg(result.targetSeconds,0,'f',3):"Auto").arg(result.seconds,0,'f',3).arg(*std::min_element(result.effectiveFrames.begin(),result.effectiveFrames.end())).arg(*std::max_element(result.effectiveFrames.begin(),result.effectiveFrames.end())));history->clear();history->hide();lastLive=true;lastDataPath=request.archive?text(request.output):"Sweep view only / SINGLE saves segments";resultAge.start();
                        df->setText(QString::number(result.spacing,'f',2)+" Hz");enbw->setText(QString::number(result.enbw,'f',2)+" Hz");tone->setText("Sequential sweep");power->setText("Not simultaneous");
                        badge->setText(QString("SWEPT / %1 segments / FFT %2 / avg %3 / %4 s").arg(result.centers.size()).arg(result.fftN).arg(result.averages).arg(result.seconds,0,'f',3));refreshCapabilities();
                        message(QString("Complete swept spectrum: %1 segments, %2 s; dBFS/bin, not full-band dBm").arg(result.centers.size()).arg(result.seconds,0,'f',3));
                    }else{repeatGate.stop();message("Sweep cancelled; incomplete round discarded, previous trace retained.");}
                    if(repeatGate.permits(token))QTimer::singleShot(0,this,[this,token]{if(repeatGate.permits(token))startSweep(true);});
                },Qt::QueuedConnection);
            }catch(const std::exception &e){const auto why=errorText(e);QMetaObject::invokeMethod(this,[this,why]{if(task.joinable())task.join();repeatGate.stop();completeUi();liveFault=true;fail("Sweep failed; previous complete trace retained: "+why);},Qt::QueuedConnection);}});
        }catch(const std::exception &e){repeatGate.stop();completeUi();fail(errorText(e));}
    }
    void start(bool repeating){
        if(busy)return;
        if(source->currentIndex()==0&&span->value()>150){startSweep(repeating);return;}
        if(repeating&&source->currentIndex()==0){startStream();return;}
        const auto repeatToken=repeatGate.begin(repeating);
        try{
            CaptureRequest request;request.centerHz=std::uint64_t(std::llround(rxCenter->value()*1e6));request.live=source->currentIndex()==0;request.n=fft->currentData().toUInt();request.frames=acquisitionFrames();
            if(request.live&&liveFault)throw std::runtime_error("Previous live request failed; reinitialize the acquisition backend before restarting the instrument.");
            if(request.live&&!rxAvailable())throw std::runtime_error("RX device unavailable; install/init the Linux acquisition backend first.");
            request.output=freshOutput("capture");request.replay=replay;
            if(!request.live&&request.replay.empty()){const auto name=request.n==1024?"lower":request.n==8192?"n8192":"n65536";request.replay={resourcesPath/"fixtures"/name/"spectrum.bin"};}
            if(!request.live&&request.replay.size()!=request.frames)throw std::runtime_error("Replay average needs one distinct packet per frame.");
            cancelled.store(false);receiving=true;setBusy(true);progress->setValue(0);lastError.clear();message("Local capture batch started.");
            if(task.joinable())task.join();
            launchTask([this,request,repeatToken]{try{
                auto result=capture(request,resources,cancelled,[this](unsigned completed,unsigned total){QMetaObject::invokeMethod(this,[this,completed,total]{progress->setValue(int(100.*completed/total));},Qt::QueuedConnection);});
                QMetaObject::invokeMethod(this,[this,repeatToken,result=std::move(result)]()mutable{if(task.joinable()){task.join();}completeUi();if(result.hasData){const auto &m=result.measurement;lastLive=result.live;lastDataPath=text(result.output);resultAge.start();plot->setData(m,result.live);history->append(m,ref->value(),yrange->value());df->setText(QString::number(m.spacing,'f',2)+" Hz");enbw->setText(QString::number(m.enbwHz,'f',2)+" Hz");tone->setText(m.toneValid?QString::number(m.toneHz/1e6,'f',6)+" MHz":"No tone estimate");power->setText(QString::number(m.totalDbfs,'f',3)+" dBFS");badge->setText(QString("%1 / FFT %2 / avg %3 / dBFS").arg(result.live?"LIVE DMA":"REPLAY").arg(m.n).arg(m.frames));saveCwSidecar(result);saveSpectrumUi(result.output/"measurement",m.frames,m.frames,m.enbwHz);refreshCapabilities();message("Saved validated acquisition: "+text(result.output));}
                    if(result.cancelled)repeatGate.stop();
                    if(autoTest){if(!result.hasData||result.measurement.n!=8192){QCoreApplication::exit(2);return;}span->setValue(40);ref->setValue(-30);yrange->setValue(100);plot->update();QTimer::singleShot(100,this,[this]{bool ok=testScreenshot.isEmpty()||grab().save(testScreenshot);QCoreApplication::exit(ok?0:2);});return;}
                    if(repeatGate.enabled())QTimer::singleShot(500,this,[this,repeatToken]{if(repeatGate.permits(repeatToken))start(true);});
                },Qt::QueuedConnection);
            }catch(const std::exception &e){const auto reason=errorText(e);QMetaObject::invokeMethod(this,[this,request,reason]{if(task.joinable())task.join();repeatGate.stop();completeUi();if(request.live)liveFault=true;fail("Capture failed; previous trace retained: "+reason);},Qt::QueuedConnection);}});
        }catch(const std::exception &e){repeatGate.stop();completeUi();fail(errorText(e));}
    }
    void showStreamFrame(const StreamUpdate &frame){
        const auto &result=frame.result;const auto &m=result.measurement;
        if(autoTest&&liveStreamTest&&vbwMode->currentIndex()==1&&m.frames!=acquisitionFrames()){cancelled.store(true);fail("VBW stream did not capture the planned independent frames");return;}
        lastLive=true;lastDataPath="Stream view only / SINGLE saves a complete record";resultAge.start();
        plot->setData(m,true);history->append(m,ref->value(),yrange->value());
        df->setText(QString::number(m.spacing,'f',2)+" Hz");enbw->setText(QString::number(m.enbwHz,'f',2)+" Hz");
        tone->setText(m.toneValid?QString::number(m.toneHz/1e6,'f',6)+" MHz":"No tone estimate");
        power->setText(QString::number(m.totalDbfs,'f',3)+" dBFS");++streamDisplayed;
        const double seconds=std::max(.001,streamElapsed.elapsed()/1000.);
        badge->setText(QString("LIVE DMA / FFT %1 / RX %2/s / display %3/s / skipped %4")
                       .arg(m.n).arg(frame.batches/seconds,0,'f',1).arg(streamDisplayed/seconds,0,'f',1).arg(qulonglong(streamLatest.replaced())));
        progress->setValue(100);refreshCapabilities();
        const int second=int(seconds);
        if(second-streamLogSecond>=5){streamLogSecond=second;
            message(QString("STREAM batches=%1 shown=%2 replaced=%3 read=%4ms validate=%5ms measure=%6ms painted=%7 avg_frames=%8; view only, not gapless IQ")
                    .arg(qulonglong(frame.batches)).arg(qulonglong(streamDisplayed)).arg(qulonglong(streamLatest.replaced()))
                    .arg(result.readMs,0,'f',2).arg(result.validateMs,0,'f',2).arg(result.measureMs,0,'f',2).arg(qulonglong(plot->paintedFrames-streamPaintBase)).arg(m.frames));}
    }
    void pollStream(){if(streaming){auto next=streamLatest.take();if(next)showStreamFrame(*next);}}
    void startStream(){
        if(span->value()>150){startSweep(true);return;}
        try{
            if(liveFault)throw std::runtime_error("Previous live request failed; reinitialize the acquisition backend first.");
            if(!rxAvailable())throw std::runtime_error("RX device unavailable; initialize the Linux backend first.");
            CaptureRequest request;request.centerHz=std::uint64_t(std::llround(rxCenter->value()*1e6));request.n=fft->currentData().toUInt();request.frames=acquisitionFrames();
            request.archive=false;repeatGate.stop();cancelled.store(false);streamLatest.clear();
            streamDisplayed=0;streamPaintBase=plot->paintedFrames;streamLogSecond=-5;streamElapsed.start();streaming=true;receiving=true;setBusy(true);lastError.clear();
            message("Continuous RX started: persistent device, latest-frame display, no automatic recording. STOP then SINGLE to save.");
            if(task.joinable())task.join();
            launchTask([this,request]{try{
                streamSpectra(request,resources,cancelled,[this](CaptureResult &&result,std::uint64_t batches){
                    streamLatest.publish(StreamUpdate{std::move(result),batches});
                });
                QMetaObject::invokeMethod(this,[this]{if(task.joinable())task.join();pollStream();streaming=false;completeUi();
                    badge->setText("LIVE STREAM STOPPED / last validated trace retained / SINGLE saves a fresh record");
                    message(QString("RX stream stopped; displayed=%1 painted=%2 seconds=%3. DMA released, no reset; no automatic recording.")
                            .arg(qulonglong(streamDisplayed)).arg(qulonglong(plot->paintedFrames-streamPaintBase)).arg(streamElapsed.elapsed()/1000.,0,'f',3));
                    if(autoTest&&liveStreamTest)QTimer::singleShot(150,this,[this]{const bool ok=streamDisplayed>=10&&(testScreenshot.isEmpty()||grab().save(testScreenshot));QCoreApplication::exit(ok?0:2);});
                },Qt::QueuedConnection);
            }catch(const std::exception &e){const auto reason=errorText(e);QMetaObject::invokeMethod(this,[this,reason]{
                if(task.joinable())task.join();
                streaming=false;streamLatest.clear();liveFault=true;completeUi();
                fail("RX stream failed; previous trace retained: "+reason);},Qt::QueuedConnection);}});
            // launchTask catches thread creation failures itself.
            if(!busy)streaming=false;
        }catch(const std::exception &e){streaming=false;completeUi();fail(errorText(e));}
    }
    void updateTxButtons(){if(!txUpload)return;txUpload->setEnabled(!busy&&txAvailable()&&!lastWaveform.isEmpty());txPlay->setEnabled(!busy&&txValidatedForControls&&(txFlags&3)==3);txStop->setEnabled(bool(tx)&&!txStopTask.joinable()&&((txFlags&4)||txBusy));refreshCapabilities();}
    std::shared_ptr<TxSession> sessionTx(){if(!tx)tx=std::make_shared<TxSession>();return tx;}
    void showTx(const TxInfo &i){txFlags=i.flags;txHealth->setText(QString("TX flags=%1 / loaded=%2 bytes / guard=0x%3 / error=%4").arg(i.flags).arg(qulonglong(i.loaded)).arg(i.guard,0,16).arg(i.error));updateTxButtons();if(rfState){rfState->setText((i.flags&4)?"RF PLAYING":"RF STOPPED");rfState->setStyleSheet((i.flags&4)?"color:#ffdc28;font-weight:bold":"color:#a8bac8");}}
    void uploadTx(){
        if(busy)return;
        repeatGate.stop();stop->setEnabled(false);
        auto filename=lastWaveform;if(filename.isEmpty()){fail("Generate a waveform or select an existing IQ file first.");return;}
        try{auto session=sessionTx();cancelled.store(false);setBusy(true);txBusy=true;updateTxButtons();progress->setValue(0);
            if(task.joinable())task.join();
            launchTask([this,session,filename]{try{
                QFile sidecar(filename+".json");if(!sidecar.open(QIODevice::ReadOnly))throw std::runtime_error("Native waveform JSON metadata is required");const auto meta=QJsonDocument::fromJson(sidecar.readAll()).object();
                if(meta["kind"].toString()!="GENERATED_IQ_NOT_PLAYING"||meta["sample_rate_hz"].toDouble()!=1.2e9||meta["bytes"].toDouble()!=48000000)throw std::runtime_error("Unsupported waveform format or record length");
                const auto hash=sha256(readFile(path(filename),64*1024*1024));if(meta["sha256"].toString()!=QString::fromStdString(hash))throw std::runtime_error("Waveform SHA256 differs from generation metadata");
                session->upload(path(filename),cancelled,[this](unsigned done,unsigned total){QMetaObject::invokeMethod(this,[this,done,total]{progress->setValue(int(100.*done/total));},Qt::QueuedConnection);});auto state=session->info();
                QMetaObject::invokeMethod(this,[this,state]{if(task.joinable()){task.join();}txValidatedForControls=true;completeUi();showTx(state);message("TX DDR round-trip and SHA256 passed; loaded, NOT playing.");},Qt::QueuedConnection);
            }catch(const std::exception&e){auto reason=errorText(e);QMetaObject::invokeMethod(this,[this,reason]{if(task.joinable())task.join();txFlags=0;completeUi();fail("TX upload failed: "+reason);},Qt::QueuedConnection);}});
        }catch(const std::exception&e){completeUi();fail(errorText(e));}
    }
    void playTx(){
        if(busy||!txValidatedForControls)return;
        try{auto session=sessionTx();setBusy(true);if(task.joinable())task.join();launchTask([this,session]{try{session->play();auto state=session->info();QMetaObject::invokeMethod(this,[this,state]{if(task.joinable()){task.join();}completeUi();showTx(state);message("DAC playback active; inspect RF/loopback measurement.");},Qt::QueuedConnection);}catch(const std::exception&e){auto reason=errorText(e);QMetaObject::invokeMethod(this,[this,reason]{if(task.joinable())task.join();txFlags=0;completeUi();fail(reason);},Qt::QueuedConnection);}});}catch(const std::exception&e){completeUi();fail(errorText(e));}
    }
    void stopTx(){
        if(!tx||txStopTask.joinable())return;
        cancelled.store(true);auto session=tx;txStop->setEnabled(false);
        try{txStopTask=std::thread([this,session]{try{session->stop();auto state=session->info();QMetaObject::invokeMethod(this,[this,state]{if(txStopTask.joinable())txStopTask.join();showTx(state);message("DAC digital playback stopped.");},Qt::QueuedConnection);}catch(const std::exception&e){auto reason=errorText(e);QMetaObject::invokeMethod(this,[this,reason]{if(txStopTask.joinable())txStopTask.join();txFlags=0;updateTxButtons();fail(reason);},Qt::QueuedConnection);}});}catch(const std::exception&e){updateTxButtons();fail(errorText(e));}
    }
    void makeWave(){
        if(busy)return;
        // A native file dialog runs a nested event loop; invalidate any queued repeat first.
        repeatGate.stop();stop->setEnabled(false);
        QString filename;try{auto target=freshOutput("wave_"+waveType->currentText().toLower());target+=".bin";filename=text(target);}catch(const std::exception&e){fail(errorText(e));return;}
        WaveRequest request;request.type=waveType->currentText().toStdString();request.order=order->currentText().toUInt();request.offset=offset->value()*1e6;request.peak=unsigned(peakCounts->value());request.output=path(filename);request.pulseWidth=pulseWidth->value()*1e-6;request.period=pulsePeriod->value()*1e-6;request.bandwidth=chirpWidth->value()*1e6;request.symbolRate=symbolRate->value()*1e6;request.rolloff=rolloff->value();
        try{if(powerMode->currentIndex()==0){if(!modelPowerReady())throw std::runtime_error("Select CW1790MHz for model dBm, or Digital / UNCALIBRATED");request.powerModel=selectedPowerModel();request.targetDbm=requestedDbm();txCwRms(*request.powerModel,rfFrequency->value()*1e6,request.targetDbm);}generatorEdited();}catch(const std::exception &e){fail(errorText(e));return;}
        cancelled.store(false);repeatGate.stop();setBusy(true);progress->setValue(0);message("Generating 10ms waveform on PS; no RF output action.");if(task.joinable())task.join();
        launchTask([this,request]{try{generateWaveform(request,cancelled,[this](unsigned done,unsigned total){QMetaObject::invokeMethod(this,[this,done,total]{progress->setValue(int(100.*done/total));},Qt::QueuedConnection);});QMetaObject::invokeMethod(this,[this,request]{if(task.joinable()){task.join();}completeUi();lastWaveform=text(request.output);waveFile->setText("Generated: "+lastWaveform);updateTxButtons();message("Waveform saved: "+lastWaveform+"; NOT PLAYING");},Qt::QueuedConnection);}catch(const std::exception &e){const auto reason=errorText(e);QMetaObject::invokeMethod(this,[this,reason]{if(task.joinable()){task.join();}completeUi();fail(reason);},Qt::QueuedConnection);}});
    }
protected:
    void closeEvent(QCloseEvent *event)override{if(busy){closing=true;repeatGate.stop();cancelled.store(true);message("Waiting for active operation to finish. The window will close after completion.");event->ignore();}else{repeatGate.stop();event->accept();}}
};
}

int main(int argc,char **argv){
    QApplication application(argc,argv);QCoreApplication::setOrganizationName("ZCU111");QCoreApplication::setApplicationName("StandaloneInstrument");application.setStyle("Fusion");application.setStyleSheet(style);
    QApplication::setOverrideCursor(instrumentCursor());
    fprintf(stdout,"INSTRUMENT_CURSOR=BUILTIN_PIXMAP\nINSTRUMENT_INTERFACE=SA3000_NATIVE_MAIN_MARKERS_16\n");fflush(stdout);
    QString resource=QCoreApplication::applicationDirPath()+"/resources",output;bool full=false,selfTest=false,generatorTest=false,streamTest=false,showGenerator=false,uploadTest=false,sweepTest=false,liveSweepTest=false,sweepView=false,powerTest=false,manualSweepTest=false,markerTest=false;int videoTestMode=0;QString screenshot;
    for(int i=1;i<argc;++i){const QString arg=QString::fromLocal8Bit(argv[i]);if(arg=="--data"&&i+1<argc)resource=QString::fromLocal8Bit(argv[++i]);else if(arg=="--output"&&i+1<argc)output=QString::fromLocal8Bit(argv[++i]);else if(arg=="--fullscreen")full=true;else if(arg=="--sweep-view")sweepView=true;else if(arg=="--generator")showGenerator=true;else if(arg=="--self-test")selfTest=true;else if(arg=="--self-test-tx-upload"){selfTest=true;uploadTest=true;}else if(arg=="--self-test-live-stream"){selfTest=true;streamTest=true;}else if(arg=="--self-test-live-manual"){selfTest=true;manualSweepTest=true;}else if(arg=="--self-test-live-sweep"){selfTest=true;liveSweepTest=true;}else if(arg=="--self-test-sweep"){selfTest=true;sweepTest=true;}else if(arg=="--self-test-live-vbw"){selfTest=true;videoTestMode=1;}else if(arg=="--self-test-live-vbw-record"){selfTest=true;videoTestMode=2;}else if(arg=="--self-test-markers"){selfTest=true;markerTest=true;}else if(arg=="--self-test-power"){selfTest=true;powerTest=true;}else if(arg=="--self-test-generator"){selfTest=true;generatorTest=true;}else if(arg=="--screenshot"&&i+1<argc)screenshot=QString::fromLocal8Bit(argv[++i]);else if(arg=="--help"){fprintf(stdout,"zcu111-instrument [--data DIR] [--output DIR] [--fullscreen] [--self-test --screenshot FILE]\n");return 0;}else{fprintf(stderr,"Unknown/incomplete argument\n");return 2;}}
    if(!QFile::exists(resource+"/window_metrics.tsv"))resource="/usr/share/zcu111-instrument";
    if(output.isEmpty())output=QSettings().value("output",QDir::tempPath()+"/zcu111_measurements_"+qEnvironmentVariable("USER","user")).toString();
    try{auto runtime=QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);if(runtime.isEmpty())runtime=QDir::tempPath();QLockFile instance(runtime+"/zcu111-instrument.lock");instance.setStaleLockTime(0);if(!instance.tryLock())throw std::runtime_error("Instrument is already running for this user");const auto id=QFontDatabase::addApplicationFont(resource+"/DejaVuSans.ttf");if(id<0)throw std::runtime_error("Cannot load bundled instrument font");application.setFont(QFont("DejaVu Sans",10));Window window(path(resource),output,selfTest,screenshot,generatorTest,streamTest,uploadTest,sweepTest,liveSweepTest,powerTest,manualSweepTest,markerTest,videoTestMode);if(sweepView)window.configureSweepView();if(showGenerator)window.selectPage(1);std::signal(SIGTERM,terminateHandler);std::signal(SIGINT,terminateHandler);QTimer shutdownPoll;QObject::connect(&shutdownPoll,&QTimer::timeout,&window,[&window]{if(terminateRequested.load())window.close();});shutdownPoll.start(200);if(full)window.showFullScreen();else window.show();return application.exec();}
    catch(const std::exception &e){fprintf(stderr,"Startup failed: %s\n",e.what());return 2;}
}
