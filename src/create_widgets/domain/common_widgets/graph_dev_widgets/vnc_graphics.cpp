#include "vnc_graphics.h"

vnc_graphHlpThread::vnc_graphHlpThread(
        QObject         *parent,
        virConnectPtr   *connPtrPtr) :
    _VirtThread(parent, connPtrPtr)
{
    qRegisterMetaType<QStringList>("QStringList&");
}
void vnc_graphHlpThread::run()
{
    if ( Q_NULLPTR==ptr_ConnPtr || Q_NULLPTR==*ptr_ConnPtr ) {
        emit ptrIsNull();
        return;
    };
    if ( virConnectRef(*ptr_ConnPtr)<0 ) {
        sendConnErrors();
        return;
    };
    QStringList nets;
    virNetworkPtr *networks = Q_NULLPTR;
    unsigned int flags =
            VIR_CONNECT_LIST_NETWORKS_ACTIVE |
            VIR_CONNECT_LIST_NETWORKS_INACTIVE;
    int ret = virConnectListAllNetworks(
                *ptr_ConnPtr, &networks, flags);
    if ( ret<0 ) {
        sendConnErrors();
    } else {
        // therefore correctly to use for() command,
        // because networks[0] can not exist.
        for (int i = 0; i < ret; i++) {
            nets.append( virNetworkGetName(networks[i]) );
            virNetworkFree(networks[i]);
        };
        if (networks) free(networks);
    };
    //int devs = virNodeNumOfDevices(ptr_ConnPtr, Q_NULLPTR, 0);
    if ( virConnectClose(*ptr_ConnPtr)<0 )
        sendConnErrors();
    emit result(nets);
}

#define KEYMAPs QStringList()<<"auto"<<"en-gb"<<"en-us"<<"ru"<<"fr"<<"de"<<"is"<<"it"<<"ja"

VNC_Graphics::VNC_Graphics(
        QWidget         *parent,
        virConnectPtr   *connPtrPtr) :
    _QWidget(parent, connPtrPtr)
{
    addrLabel = new QLabel(tr("Address:"), this);
    address = new QComboBox(this);
    address->setEditable(false);
    address->addItem(tr("HyperVisor default"), "");
    address->addItem(tr("LocalHost only"), "127.0.0.1");
    address->addItem(tr("All Interfaces"), "0.0.0.0");
    address->addItem(tr("Custom"), "custom");
    address->addItem(tr("Use named configured Network"), "network");
    address->addItem(tr("Use Socket"), "socket");
    address->insertSeparator(4);
    address->insertSeparator(6);
    networks = new QComboBox(this);
    networks->setVisible(false);
    autoPort = new QCheckBox(tr("AutoPort"), this);
    port = new QSpinBox(this);
    port->setRange(1000, 65535);
    port->setValue(5900);
    port->setEnabled(false);
    usePassw = new QCheckBox(tr("Password"), this);
    passw = new QLineEdit(this);
    passw->setEnabled(false);
    keymapLabel = new QLabel(tr("Keymap:"), this);
    keymap = new QComboBox(this);
    keymap->setEditable(true);
    keymap->addItems(KEYMAPs);
    keymap->setEnabled(false);
    shareLabel = new QLabel(tr("Share policy:"), this);
    sharePolicy = new QComboBox(this);
    sharePolicy->addItem(
                tr("Multiple clients (default)"), "");
    sharePolicy->addItem(
                tr("Exclusive access"), "allow-exclusive");
    sharePolicy->addItem(
                tr("Disable exclusive client access"), "force-shared");
    sharePolicy->addItem(
                tr("Welcomes every connection unconditionally"), "ignore");
    commonLayout = new QGridLayout();
    commonLayout->addWidget(addrLabel, 0, 0);
    commonLayout->addWidget(address, 0, 1);
    commonLayout->addWidget(networks, 1, 1);
    commonLayout->addWidget(autoPort, 2, 0);
    commonLayout->addWidget(port, 2, 1);
    commonLayout->addWidget(usePassw, 3, 0);
    commonLayout->addWidget(passw, 3, 1);
    commonLayout->addWidget(keymapLabel, 4, 0);
    commonLayout->addWidget(keymap, 4, 1);
    commonLayout->addWidget(shareLabel, 5, 0, Qt::AlignTop);
    commonLayout->addWidget(sharePolicy, 5, 1, Qt::AlignTop);
    setLayout(commonLayout);
    connect(address, SIGNAL(currentIndexChanged(int)),
            this, SLOT(addressEdit(int)));
    connect(autoPort, SIGNAL(toggled(bool)),
            this, SLOT(usePort(bool)));
    connect(usePassw, SIGNAL(toggled(bool)),
            this, SLOT(usePassword(bool)));
    autoPort->setChecked(true);
    usePassw->setChecked(false);
    hlpThread = new vnc_graphHlpThread(this, connPtrPtr);
    connect(hlpThread, SIGNAL(result(QStringList&)),
            this, SLOT(readNetworkList(QStringList&)));
    connect(hlpThread, SIGNAL(errorMsg(const QString&, const uint)),
            this, SIGNAL(errorMsg(const QString&)));
    connect(hlpThread, SIGNAL(finished()),
            this, SLOT(emitCompleteSignal()));
    hlpThread->start();
    // dataChanged connections
    connect(address, SIGNAL(currentIndexChanged(int)),
            this, SLOT(stateChanged()));
    connect(networks, SIGNAL(currentIndexChanged(int)),
            this, SLOT(stateChanged()));
    connect(autoPort, SIGNAL(toggled(bool)),
            this, SLOT(stateChanged()));
    connect(port, SIGNAL(valueChanged(int)),
            this, SLOT(stateChanged()));
    connect(usePassw, SIGNAL(toggled(bool)),
            this, SLOT(stateChanged()));
    connect(passw, SIGNAL(textEdited(QString)),
            this, SLOT(stateChanged()));
    connect(keymap, SIGNAL(currentIndexChanged(int)),
            this, SLOT(stateChanged()));
    connect(sharePolicy, SIGNAL(currentIndexChanged(int)),
            this, SLOT(stateChanged()));
}

/* public slots */
QDomDocument VNC_Graphics::getDataDocument() const
{
    QDomDocument doc;
    QDomElement _listen, _device, _devDesc;
    _device = doc.createElement("device");
    _devDesc = doc.createElement("graphics");
    _devDesc.setAttribute("type", "vnc");
    QString _sharePolicy = sharePolicy->itemData(
                sharePolicy->currentIndex(), Qt::UserRole).toString();
    if ( !_sharePolicy.isEmpty() )
        _devDesc.setAttribute("sharePolicy", _sharePolicy);
    if ( autoPort->isEnabled() ) {
        if ( autoPort->isChecked() ) {
            _devDesc.setAttribute("autoport", "yes");
            _devDesc.setAttribute("port", "-1");
        } else {
            _devDesc.setAttribute("port", port->text());
        };
    };
    if ( usePassw->isChecked() ) {
        _devDesc.setAttribute("passwd", passw->text());
        _devDesc.setAttribute("keymap", keymap->currentText());
    };
    QString _address = address->itemData(
                address->currentIndex(), Qt::UserRole).toString();
    if ( !_address.isEmpty()
         && _address.compare("network")!=0
         && _address.compare("socket")!=0 ) {
        _listen = doc.createElement("listen");
        _listen.setAttribute("type", "address");
        if ( _address.compare("custom")!=0 ) {
            _listen.setAttribute("address", _address);
        } else {
            _listen.setAttribute(
                        "address",
                        address->currentText());
        };
        _devDesc.appendChild(_listen);
    } else if ( _address.compare("network")==0 && networks->count()>0 ) {
            _listen = doc.createElement("listen");
            _listen.setAttribute("type", "network");
            _listen.setAttribute("network", networks->currentText());
            _devDesc.appendChild(_listen);
    } else if ( _address.compare("socket")==0 ) {
        _devDesc.setAttribute("socket", address->currentText());
        _listen = doc.createElement("listen");
        _listen.setAttribute("type", "socket");
        _listen.setAttribute("socket", address->currentText());
        _devDesc.appendChild(_listen);
    };
    _device.appendChild(_devDesc);
    doc.appendChild(_device);
    return doc;
}
void VNC_Graphics::setDataDescription(const QString &_xmlDesc)
{
    //qDebug()<<_xmlDesc;
    QDomDocument doc;
    QDomElement _device, _listen;
    doc.setContent(_xmlDesc);
    _device = doc.firstChildElement("device")
            .firstChildElement("graphics");
    QString _sharePolicy = _device.attribute("sharePolicy");
    int idx = sharePolicy->findData(
                _sharePolicy, Qt::UserRole, Qt::MatchExactly);
    sharePolicy->setCurrentIndex( (idx<0)? 0:idx );
    autoPort->setChecked(
                _device.attribute("autoport").compare("yes")==0);
    if ( !autoPort->isChecked() ) {
        port->setValue(
                    _device.attribute("port").toInt());
    };
    usePassw->setChecked( _device.hasAttribute("passwd") );
    if ( _device.hasAttribute("passwd") ) {
        QString _password = _device.attribute("passwd");
        passw->setText(_password);
        idx = keymap->findText(
                    _device.attribute("keymap"),
                    Qt::MatchContains);
        keymap->setCurrentIndex( (idx<0)? 0:idx );
    };
    if ( _device.hasAttribute("socket") ) {
        idx = address->findData(
                    "socket", Qt::UserRole, Qt::MatchExactly);
        address->setCurrentIndex( (idx<0)? 3:idx );
        address->setEditText(_device.attribute("socket"));
    } else {
        _listen = _device.firstChildElement("listen");
    };
    if ( !_listen.isNull() ) {
        QString _type, _data;
        _type = _listen.attribute("type");
        _data = _listen.attribute(_type);
        if ( !_type.isEmpty() ) {
            idx = address->findData(
                        _type, Qt::UserRole, Qt::MatchExactly);
            address->setCurrentIndex( (idx<0)? 3:idx );
            if ( _type.compare("address")==0 ) {
                if ( address->currentIndex()==3 )
                    address->setEditText(_data);
            } else if ( _type.compare("network")==0 ) {
                idx = networks->findText(
                            _data,
                            Qt::MatchContains);
                networks->setCurrentIndex( (idx<0)? 0:idx );
            } else if ( _type.compare("socket")==0 ) {
                address->setEditText(_data);
            } else {
                address->setCurrentIndex(0);
            };
        } else {
            address->setCurrentIndex(0);
        };
    };
}

/* private slots */
void VNC_Graphics::usePort(bool state)
{
    port->setEnabled(!state);
}
void VNC_Graphics::usePassword(bool state)
{
    passw->setEnabled(state);
    keymap->setEnabled(state);
}
void VNC_Graphics::addressEdit(int i)
{
    QString s = address->itemData(i, Qt::UserRole).toString();
    if ( s.compare("network")==0 ) {
        address->setEditable(false);
        addrLabel->setText(tr("Network:"));
        networks->setVisible(true);
        autoPort->setEnabled(true);
        port->setEnabled(true);
    } else if ( s.compare("socket")==0 ) {
        addrLabel->setText(tr("Socket:"));
        address->setEditable(true);
        address->clearEditText();
        networks->setVisible(false);
        autoPort->setEnabled(false);
        port->setEnabled(false);
    } else {
        addrLabel->setText(tr("Address:"));
        if ( s.compare("custom")==0 ) {
            address->setEditable(true);
            address->clearEditText();
        } else {
            address->setEditable(false);
        };
        networks->setVisible(false);
        autoPort->setEnabled(true);
        port->setEnabled(true);
    }
}
void VNC_Graphics::readNetworkList(QStringList &_nets)
{
    nets = _nets;
    networks->addItems(nets);
}
void VNC_Graphics::emitCompleteSignal()
{
    if ( sender()==hlpThread ) {
        setEnabled(true);
        emit complete();
    }
}
