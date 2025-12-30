#include "MainWindow.h"

#include "WWAudio.h"
#include "assetmgr.h"
#include "wwmath.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("OpenW3D");
    QCoreApplication::setApplicationName("W3DViewQt");

    WWMath::Init();

    WW3DAssetManager asset_manager;
    asset_manager.Set_WW3D_Load_On_Demand(true);

    WWAudioClass *audio_mgr = WWAudioClass::Create_Instance();
    if (audio_mgr) {
        audio_mgr->Initialize();
    }

    W3DViewMainWindow window;
    window.show();

    const int result = app.exec();

    if (audio_mgr) {
        audio_mgr->Shutdown();
    }

    WWMath::Shutdown();
    return result;
}
