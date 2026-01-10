#include "TgSmallRawDataProcessDialog.h"

TgSmallRawDataProcessDialog::TgSmallRawDataProcessDialog(QWidget *parent,
                                                         AppInitializer* appInitializer,
                                                         DataNavigator *mainNavigator)
    : TgSmallDataProcessDialog(parent,
                               appInitializer,
                               mainNavigator,
                               QStringLiteral("小热重（原始数据）"))
{
}
