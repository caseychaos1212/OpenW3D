#include "ResolutionDialog.h"

#include "rddesc.h"
#include "ww3d.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QSettings>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QtGlobal>

namespace {
constexpr int kRoleWidth = Qt::UserRole + 1;
constexpr int kRoleHeight = Qt::UserRole + 2;
constexpr int kRoleBpp = Qt::UserRole + 3;
} // namespace

ResolutionDialog::ResolutionDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Change Resolution");

    auto *layout = new QVBoxLayout(this);
    auto *hint = new QLabel(
        "Use the controls below to select a resolution for this application. "
        "Note: This setting has no effect in Windowed mode.",
        this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    _table = new QTableWidget(this);
    _table->setColumnCount(2);
    _table->setHorizontalHeaderLabels(QStringList() << "Resolution"
                                                    << "Bit Depth");
    _table->horizontalHeader()->setStretchLastSection(true);
    _table->setSelectionBehavior(QAbstractItemView::SelectRows);
    _table->setSelectionMode(QAbstractItemView::SingleSelection);
    _table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(_table, &QTableWidget::cellDoubleClicked, this, &ResolutionDialog::onDoubleClicked);
    layout->addWidget(_table);

    _fullscreenCheck = new QCheckBox("&Fullscreen", this);
    QSettings settings;
    const int windowed = settings.value("Config/Windowed", 1).toInt();
    _fullscreenCheck->setChecked(windowed == 0);
    layout->addWidget(_fullscreenCheck);

    const RenderDeviceDescClass &device_info = WW3D::Get_Render_Device_Desc();
    const DynamicVectorClass<ResolutionDescClass> &res_list = device_info.Enumerate_Resolutions();

    int curr_width = 0;
    int curr_height = 0;
    int curr_bpp = 0;
    bool curr_windowed = true;
    WW3D::Get_Device_Resolution(curr_width, curr_height, curr_bpp, curr_windowed);

    _table->setRowCount(res_list.Count());
    int selected_row = -1;
    for (int index = 0; index < res_list.Count(); ++index) {
        const int width = res_list[index].Width;
        const int height = res_list[index].Height;
        const int bpp = res_list[index].BitDepth;

        auto *res_item = new QTableWidgetItem(QString("%1 x %2").arg(width).arg(height));
        res_item->setData(kRoleWidth, width);
        res_item->setData(kRoleHeight, height);
        res_item->setData(kRoleBpp, bpp);
        _table->setItem(index, 0, res_item);

        const quint64 colors = (bpp >= 0 && bpp < 63) ? (quint64(1) << bpp) : 0;
        auto *bpp_item =
            new QTableWidgetItem(QString("%1 bpp (%2 colors)").arg(bpp).arg(colors));
        _table->setItem(index, 1, bpp_item);

        if (selected_row < 0 && width == curr_width && height == curr_height && bpp == curr_bpp) {
            selected_row = index;
        }
    }

    if (selected_row < 0) {
        selected_row = 0;
    }
    if (selected_row >= 0 && selected_row < _table->rowCount()) {
        _table->selectRow(selected_row);
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

int ResolutionDialog::selectedWidth() const
{
    const int row = _table ? _table->currentRow() : -1;
    if (row < 0) {
        return 0;
    }

    const auto *item = _table->item(row, 0);
    return item ? item->data(kRoleWidth).toInt() : 0;
}

int ResolutionDialog::selectedHeight() const
{
    const int row = _table ? _table->currentRow() : -1;
    if (row < 0) {
        return 0;
    }

    const auto *item = _table->item(row, 0);
    return item ? item->data(kRoleHeight).toInt() : 0;
}

int ResolutionDialog::selectedBitsPerPixel() const
{
    const int row = _table ? _table->currentRow() : -1;
    if (row < 0) {
        return 0;
    }

    const auto *item = _table->item(row, 0);
    return item ? item->data(kRoleBpp).toInt() : 0;
}

bool ResolutionDialog::fullscreen() const
{
    return _fullscreenCheck && _fullscreenCheck->isChecked();
}

void ResolutionDialog::onDoubleClicked(int row, int column)
{
    Q_UNUSED(row);
    Q_UNUSED(column);
    accept();
}
