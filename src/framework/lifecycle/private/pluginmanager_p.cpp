/*
 * Copyright (C) 2020 ~ 2022 Uniontech Software Technology Co., Ltd.
 *
 * Author:     huanyu<huanyub@uniontech.com>
 *
 * Maintainer: zhengyouge<zhengyouge@uniontech.com>
 *             yanghao<yanghao@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "listener/listener.h"
#include "listener/private/listener_p.h"
#include "pluginmanager_p.h"
#include "lifecycle/private/pluginmetaobject_p.h"
#include "lifecycle/plugin.h"

#include "log/frameworklog.h"

DPF_BEGIN_NAMESPACE

namespace GlobalPrivate {
    static QMutex mutex;
} // namespace GlobalPrivate

PluginManagerPrivate::PluginManagerPrivate(PluginManager *qq)
    : q(qq)
{
    dpfCheckTimeBegin();
    dpfCheckTimeEnd();
}

PluginManagerPrivate::~PluginManagerPrivate()
{
    stopPlugins();
}

QString PluginManagerPrivate::pluginIID() const
{
    return pluginLoadIID;
}

void PluginManagerPrivate::setPluginIID(const QString &pluginIID)
{
    pluginLoadIID = pluginIID;
}

QStringList PluginManagerPrivate::pluginPaths() const
{
    return pluginLoadPaths;
}

void PluginManagerPrivate::setPluginPaths(const QStringList &pluginPaths)
{
    pluginLoadPaths = pluginPaths;
}

QStringList PluginManagerPrivate::servicePaths() const
{
    return serviceLoadPaths;
}

void PluginManagerPrivate::setServicePaths(const QStringList &servicePaths)
{
    serviceLoadPaths = servicePaths;
}

void PluginManagerPrivate::setPluginEnable(const PluginMetaObject &meta, bool enabled)
{
    return setting.setPluginEnable(meta,enabled);
}

/*!
 * \brief 获取插件的元数据，线程安全
 * \param name
 * \param version
 * \return
 */
PluginMetaObjectPointer PluginManagerPrivate::pluginMetaObj(const QString &name,
                                                            const QString &version)
{
    dpfCheckTimeBegin();

    auto controller = QtConcurrent::run([=](){
        QMutexLocker lock(&GlobalPrivate::mutex);
        int size = readQueue.size();
        int idx = 0;
        while (idx < size) {
            if (!version.isEmpty()) {
                if (readQueue[idx]->d->version == version
                        && readQueue[idx]->d->name == name) {
                    return readQueue[idx];
                }
            } else {
                if (readQueue[idx]->d->name == name) {
                    return readQueue[idx];
                }
            }
            idx++;
        }
        return PluginMetaObjectPointer(nullptr);
    });
    controller.waitForFinished();

    dpfCheckTimeBegin();

    return controller.result();
}

/*!
 * \brief 加载一个插件，线程安全，可单独使用
 * \param pluginMetaObj
 */
bool PluginManagerPrivate::loadPlugin(PluginMetaObjectPointer &pluginMetaObj)
{
    dpfCheckTimeBegin();

    QMutexLocker lock(&GlobalPrivate::mutex);

    //流程互斥
    if (pluginMetaObj->d->state >= PluginMetaObject::State::Loaded) {
        dpfDebug() << "Plugin"
                   << pluginMetaObj->d->name
                   << "already loaded and state: "
                   << pluginMetaObj->d->state;
        return true;
    }

    if (pluginMetaObj.isNull()) {
        dpfCritical() << "Failed, load plugin is nullptr";
        return false;
    }

    bool result = pluginMetaObj->d->loader->load();
    pluginMetaObj->d->plugin = QSharedPointer<Plugin>(
                qobject_cast<Plugin*>(pluginMetaObj->d->loader->instance()));
    if (!pluginMetaObj->d->plugin.isNull()) {
        pluginMetaObj->d->state = PluginMetaObject::State::Loaded;
        dpfDebug() << "Loaded plugin: " << pluginMetaObj->d->name;
    } else {
        pluginMetaObj->d->error = "Failed, get plugin instance is nullptr: "
                + pluginMetaObj->d->loader->errorString();
        dpfCritical() << pluginMetaObj->d->error;
        result = false;
    }

    dpfCheckTimeEnd();

    return result;
}

/*!
 * \brief 初始化一个插件，线程安全，可单独使用
 * \param pluginMetaObj
 */
bool PluginManagerPrivate::initPlugin(PluginMetaObjectPointer &pluginMetaObj)
{
    dpfCheckTimeBegin();

    //流程互斥
    if (pluginMetaObj->d->state >= PluginMetaObject::State::Initialized) {
        dpfCritical() << "Plugin"
                      << pluginMetaObj->d->name
                      << "already initialized and state: "
                      << pluginMetaObj->d->state;
        return true;
    }

    if (pluginMetaObj.isNull()) {
        dpfCritical() << "Failed, init plugin is nullptr";
        return false;
    }

    auto pluginInterface = pluginMetaObj->plugin();

    if (pluginInterface.isNull()) {
        dpfCritical() << "Failed, init plugin interface is nullptr";
        return false;
    }

    //线程互斥
    QMutexLocker lock(&GlobalPrivate::mutex);

    pluginMetaObj->d->plugin->initialize();
    dpfDebug() << "Initialized plugin: " << pluginMetaObj->d->name;
    pluginMetaObj->d->state = PluginMetaObject::State::Initialized;

    dpfCheckTimeEnd();
    return true;
}

/*!
 * \brief 启动一个插件，线程安全，可单独使用
 * \param pluginMetaObj
 */
bool PluginManagerPrivate::startPlugin(PluginMetaObjectPointer &pluginMetaObj)
{
    dpfCheckTimeBegin();

    //流程互斥
    if (pluginMetaObj->d->state >= PluginMetaObject::State::Started) {
        dpfCritical() << "State error: " << pluginMetaObj->d->state;
        return false;
    }

    if (pluginMetaObj.isNull()) {
        dpfCritical() << "Failed, start plugin is nullptr";
        return false;
    }

    auto pluginInterface = pluginMetaObj->plugin();

    if (pluginInterface.isNull()) {
        dpfCritical() << "Failed, start plugin interface is nullptr";
        return false;
    }

    //线程互斥
    QMutexLocker lock(&GlobalPrivate::mutex);

    if (pluginMetaObj->d->plugin->start()) {
        pluginMetaObj->d->state = PluginMetaObject::State::Started;
        dpfDebug() << "Started plugin: " << pluginMetaObj->d->name;
    } else {
        pluginMetaObj->d->error = "Failed, start plugin in function start() logic";
        dpfCritical() << pluginMetaObj->d->error.toLocal8Bit().data();
    }

    dpfCheckTimeEnd();
    return true;
}

/*!
 * \brief 停止并卸载一个插件，线程安全，可单独使用
 * \param pluginMetaObj
 */
void PluginManagerPrivate::stopPlugin(PluginMetaObjectPointer &pluginMetaObj)
{
    dpfCheckTimeBegin();

    //流程互斥
    if (pluginMetaObj->d->state >= PluginMetaObject::State::Stoped)
        return;

    //线程互斥
    QMutexLocker lock(&GlobalPrivate::mutex);

    Plugin::ShutdownFlag stFlag = pluginMetaObj->d->plugin->stop();
    pluginMetaObj->d->state = PluginMetaObject::State::Stoped;

    if (stFlag == Plugin::ShutdownFlag::Async) {

        dpfDebug() << "async stop" << pluginMetaObj->d->plugin->
                      metaObject()->className();

        pluginMetaObj->d->state = PluginMetaObject::State::Stoped;

        QObject::connect(pluginMetaObj->d->plugin.data(), &Plugin::asyncStopFinished,
                         pluginMetaObj->d->plugin.data(), [=]() {
            pluginMetaObj->d->plugin = nullptr;

            if (!pluginMetaObj->d->loader->unload()) {
                dpfDebug() << pluginMetaObj->d->loader->errorString();
            }

            pluginMetaObj->d->state = PluginMetaObject::State::Shutdown;
            dpfDebug() << "shutdown" << pluginMetaObj->d->loader->fileName();

        },Qt::ConnectionType::DirectConnection); //同步信号直接调用无需等待

    } else {

        if (pluginMetaObj->d->plugin) {
            dpfDebug() << "sync stop" << pluginMetaObj->d->plugin->
                          metaObject()->className();

            pluginMetaObj->d->plugin = nullptr;
            pluginMetaObj->d->state = PluginMetaObject::State::Stoped;
        }

        if (!pluginMetaObj->d->loader->unload()) {
            dpfDebug() << pluginMetaObj->d->loader->errorString();
        }

        pluginMetaObj->d->state = PluginMetaObject::State::Shutdown;
        dpfDebug() << "shutdown" << pluginMetaObj->d->loader->fileName();
    }

    dpfCheckTimeEnd();
}

/*!
 * \brief 读取所有插件的Json源数据
 * \return 如果未读到任何插件则false
 */
bool PluginManagerPrivate::readPlugins()
{
    dpfCheckTimeBegin();

    // 内部已有线程互斥
    scanfAllPlugin(readQueue, pluginLoadPaths, pluginLoadIID);

    QMutexLocker lock(&GlobalPrivate::mutex);
    int currMaxCountThread = QThreadPool::globalInstance()->maxThreadCount();
    if (currMaxCountThread < 4) {
        QThreadPool::globalInstance()->setMaxThreadCount(4);
    }

    // 并发读取数据
    QFuture<void> mapController = QtConcurrent::map(readQueue.begin(),
                                                    readQueue.end(),
                                                    readJsonToMeta);
    mapController.waitForFinished();
    dpfDebug() << readQueue;

    dpfCheckTimeEnd();
    return readQueue.isEmpty() ? false : true;
}

/*!
 * \brief 扫描所有插件到目标队列
 * \param destQueue 目标队列
 * \param pluginPaths 插件路径列表
 * \param pluginIID 插件身份
 */
void PluginManagerPrivate::scanfAllPlugin(PluginMetaQueue &destQueue,
                                          const QStringList &pluginPaths,
                                          const QString &pluginIID)
{
    dpfCheckTimeBegin();

    if (pluginIID.isEmpty())
        return;

    for(const QString &path : pluginPaths) {
        QDirIterator dirItera(path, {"*.so", "*.dll"},
                              QDir::Filter::Files,
                              QDirIterator::IteratorFlag::NoIteratorFlags);

        //线程安全
        QMutexLocker lock(&GlobalPrivate::mutex);

        while (dirItera.hasNext()) {
            dirItera.next();

            PluginMetaObjectPointer metaObj(new PluginMetaObject);
            metaObj->d->loader->setFileName(dirItera.path() + QDir::separator() + dirItera.fileName());
            QJsonObject &&metaJson = metaObj->d->loader->metaData();
            QString &&IID = metaJson.value("IID").toString();
            if (pluginIID != IID) {
                continue;
            }

            destQueue.append(metaObj);
            metaObj->d->state = PluginMetaObject::Readed;
        }
    }

    dpfCheckTimeEnd();
}

/*!
 * \brief 同步json到定义类型
 * \param metaObject
 */
void PluginManagerPrivate::readJsonToMeta(const PluginMetaObjectPointer &metaObject)
{
    dpfCheckTimeBegin();

    metaObject->d->state = PluginMetaObject::Reading;

    QJsonObject &&jsonObj = metaObject->d->loader->metaData();

    if (jsonObj.isEmpty()) return;

    QString &&iid = jsonObj.value("IID").toString();
    if (iid.isEmpty()) return;
    metaObject->d->iid = iid;

    QJsonObject &&metaData = jsonObj.value("MetaData").toObject();

    QString &&name = metaData.value(PLUGIN_NAME).toString();
    if (name.isEmpty()) return;
    metaObject->d->name = name;

    QString &&version = metaData.value(PLUGIN_VERSION).toString();
    metaObject->d->version = version;

    QString &&compatVersion = metaData.value(PLUGIN_COMPATVERSION).toString();
    metaObject->d->compatVersion = compatVersion;

    QString &&category = metaData.value(PLUGIN_CATEGORY).toString();
    metaObject->d->category = category;

    QJsonArray &&licenseArray = metaData.value(PLUGIN_LICENSE).toArray();
    auto licenItera = licenseArray.begin();
    while (licenItera != licenseArray.end()) {
        metaObject->d->license.append(licenItera->toString());
        ++ licenItera;
    }

    QString &&copyright = metaData.value(PLUGIN_COPYRIGHT).toString();
    metaObject->d->copyright = copyright;

    QString &&vendor = metaData.value(PLUGIN_VENDOR).toString();
    metaObject->d->vendor = vendor;

    QString &&description = metaData.value(PLUGIN_DESCRIPTION).toString();
    metaObject->d->description = description;

    QString &&urlLink = metaData.value(PLUGIN_URLLINK).toString();
    metaObject->d->urlLink = urlLink;

    QJsonArray &&dependsArray = metaData.value(PLUGIN_DEPENDS).toArray();
    auto itera = dependsArray.begin();
    while (itera != dependsArray.end()) {
        QJsonObject &&dependObj = itera->toObject();
        QString &&dependName = dependObj.value(PLUGIN_NAME).toString();
        QString &&dependVersion = dependObj.value(PLUGIN_VERSION).toString();
        PluginDepend depends;
        depends.pluginName = dependName;
        depends.pluginVersion = dependVersion;
        metaObject->d->depends.append(depends);
        ++ itera;
    }

    metaObject->d->state = PluginMetaObject::Readed;

    dpfCheckTimeEnd();
}

/*!
 * \brief PluginManagerPrivate::dependsSort 插件依赖排序算法
 * \param srcQueue 传入需要排序的插件源队列
 * \return 返回排序后的Queue
 */
PluginManagerPrivate::PluginMetaQueue PluginManagerPrivate::dependsSort(
        const PluginManagerPrivate::PluginMetaQueue &srcQueue)
{
    dpfCheckTimeBegin();

    QMutexLocker lock(&GlobalPrivate::mutex);

    PluginMetaQueue result;

    if (srcQueue.isEmpty())
        return result;

    PluginMetaQueue temp = srcQueue;
    auto itera = temp.begin();
    while (itera != temp.end()) {
        if ((*itera)->depends().isEmpty()) {
            result.append(*itera);
            itera = temp.erase(itera);
            continue;
        }
        itera ++;
    }

    auto dependsNoContains = [=](const PluginMetaQueue &src, const PluginMetaObjectPointer &elem) {
        QList<QString> result;
        QList<QString> srcPluginNames;
        for (auto val : src) {
            srcPluginNames << val->name();
        }

        for (auto depend : elem->depends()) {
            if (!srcPluginNames.contains(depend.name())) {
                result << depend.name();
            }
        }
        return result;
    };

    while (!temp.isEmpty()) {
        auto itera = temp.begin();
        while (itera != temp.end()) {
            auto srcNoContains = dependsNoContains(srcQueue, *itera);
            auto dstNoContains = dependsNoContains(result, *itera);
            if (!srcNoContains.isEmpty()) {
                qCritical() << "Error, invalid plugin depends: " << srcNoContains
                            << " from plugin: " << (*itera)->name();
                for (auto noContainsVal : srcNoContains) {
                    // Delete not existen plugin depend from plugin-name
                    dstNoContains.removeOne(noContainsVal);
                }
                // claer not existen plugin depend from plugin-name
                srcNoContains.clear();
            }

            if (dstNoContains.isEmpty()) {
                result.append(*itera);
                itera = temp.erase(itera);
                continue;
            }
            itera ++;
        }
    }

    dpfCheckTimeEnd();

    return result;
}

/*!
 * \brief 内部使用QPluginLoader加载所有插件
 */
bool PluginManagerPrivate::loadPlugins()
{
    dpfCheckTimeBegin();

    loadQueue = dependsSort(readQueue);
    bool ret = true;

    for (auto val : loadQueue) {
        if (val->depends().isEmpty()) {
            QtConcurrent::run(this, &PluginManagerPrivate::loadPlugin, val);
        } else {
            ret &= loadPlugin(val);
        }
    }

    dpfDebug() << loadQueue;
    dpfCheckTimeEnd();
    return ret;
}

/*!
 * \brief 初始化所有插件
 */
void PluginManagerPrivate::initPlugins()
{
    dpfCheckTimeBegin();

    QtConcurrent::map(loadQueue.begin(), loadQueue.end(), [=](auto pointer){
        this->initPlugin(pointer);
    }).waitForFinished();

    // 私有类转发进行Sendler闭包
    emit Listener::instance().d->pluginsInitialized();

    dpfCheckTimeEnd();
}

/*!
 * \brief 拉起插件,仅主线程使用
 */
void PluginManagerPrivate::startPlugins()
{
    dpfCheckTimeBegin();

    for (auto val: loadQueue) {
        startPlugin(val);
    }

    // 私有类转发进行Sendler闭包
    emit Listener::instance().d->pluginsStarted();

    dpfCheckTimeEnd();
}

/*!
 * \brief 停止插件,仅主线程
 */
void PluginManagerPrivate::stopPlugins()
{
    dpfCheckTimeBegin();

    auto itera = loadQueue.rbegin();
    while(itera != loadQueue.rend()) {
        stopPlugin(*itera);
        ++ itera;
    }

    // 私有类转发进行Sendler闭包
    emit Listener::instance().d->pluginsStoped();

    dpfCheckTimeEnd();
}

DPF_END_NAMESPACE