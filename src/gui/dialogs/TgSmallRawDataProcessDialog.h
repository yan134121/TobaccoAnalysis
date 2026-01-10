#ifndef TGSMALLRAWDATAPROCESSDIALOG_H
#define TGSMALLRAWDATAPROCESSDIALOG_H

#include "gui/dialogs/TgSmallDataProcessDialog.h"

class TgSmallRawDataProcessDialog : public TgSmallDataProcessDialog
{
    Q_OBJECT

public:
    explicit TgSmallRawDataProcessDialog(QWidget *parent = nullptr,
                                         AppInitializer* appInitializer = nullptr,
                                         DataNavigator *mainNavigator = nullptr);
    ~TgSmallRawDataProcessDialog() override = default;
};

#endif // TGSMALLRAWDATAPROCESSDIALOG_H
