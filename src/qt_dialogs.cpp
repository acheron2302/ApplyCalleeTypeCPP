// qt_dialogs.cpp
// Qt dialogs implementation for ApplyCalleeTypeEx plugin

#include "qt_dialogs.hpp"

#include <qboxlayout.h>
#include <qfontdatabase.h>
#include <qheaderview.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qlistwidget.h>
#include <qplaintextedit.h>
#include <qpushbutton.h>
#include <qsplitter.h>
#include <qstring.h>
#include <qtablewidget.h>
#include <qwindowdefs.h>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include <optional>

// IDA SDK headers for type enumeration (loaded by idax)
#include <ida/idax.hpp>
#include <ida/type.hpp>

namespace {

// Use QT:: prefix since IDA SDK Qt uses QT_NAMESPACE=QT
QT::QString to_qstring(std::string_view sv) {
    return QT::QString::fromLatin1(sv.data(), static_cast<int>(sv.size()));
}

QT::QString format_ea(uint64_t ea) {
    return QT::QString("0x%1").arg(ea, 0, 16);
}

// ============================================================================
// Type population helpers using idax
// ============================================================================

void populate_til_types_impl(QT::QListWidget* list) {
    list->clear();
    
    auto result = ida::type::named_types();
    if (!result) {
        auto* item = new QT::QListWidgetItem("(TIL enumeration failed)");
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        list->addItem(item);
        return;
    }
    
    for (const auto& entry : *result) {
        list->addItem(to_qstring(entry.name));
    }
    
    if (list->count() == 0) {
        auto* item = new QT::QListWidgetItem("(No TIL types available)");
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        list->addItem(item);
    }
}

void populate_local_types_impl(QT::QListWidget* list) {
    list->clear();
    
    try {
        // Use idax API for local type enumeration
        auto count_result = ida::type::local_type_count();
        if (!count_result) {
            auto* item = new QT::QListWidgetItem("(Error getting local type count)");
            item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
            list->addItem(item);
            return;
        }
        
        std::size_t count = *count_result;
        if (count == 0) {
            auto* item = new QT::QListWidgetItem("(No local types in database)");
            item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
            list->addItem(item);
            return;
        }
        
        for (std::size_t ordinal = 1; ordinal <= count; ++ordinal) {
            try {
                auto name_result = ida::type::local_type_name(ordinal);
                if (name_result) {
                    auto* item = new QT::QListWidgetItem(to_qstring(*name_result));
                    list->addItem(item);
                }
            } catch (...) {
                // Skip this ordinal on error
            }
        }
    } catch (const std::exception&) {
        auto* item = new QT::QListWidgetItem("(Error enumerating local types)");
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        list->addItem(item);
    } catch (...) {
        auto* item = new QT::QListWidgetItem("(Unknown error during enumeration)");
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        list->addItem(item);
    }
}

} // anonymous namespace

void populate_til_types(QListWidget* list) {
    populate_til_types_impl(static_cast<QT::QListWidget*>(list));
}

void populate_local_types(QListWidget* list) {
    populate_local_types_impl(static_cast<QT::QListWidget*>(list));
}

// ============================================================================
// TypeSourceDialog
// ============================================================================

TypeSourceDialog::TypeSourceDialog(QWidget* parent, uint64_t ea)
    : QT::QDialog(parent), ea_(ea) {
    setWindowTitle(QT::QString("ApplyCalleeTypeEx - %1").arg(format_ea(ea)));
    setMinimumWidth(400);
    setModal(true);
    setup_ui();
}

TypeSourceDialog::~TypeSourceDialog() = default;

void TypeSourceDialog::setup_ui() {
    auto* layout = new QT::QVBoxLayout(this);

    auto* label = new QT::QLabel(QT::QString("Select prototype source for call at %1:").arg(format_ea(ea_)));
    layout->addWidget(label);
    layout->addSpacing(12);

    auto* manual_btn = new QT::QPushButton("Enter Manually...");
    manual_btn->setToolTip("Open multi-line prototype editor");
    layout->addWidget(manual_btn);

    auto* standard_btn = new QT::QPushButton("Standard Type  (TIL)");
    standard_btn->setToolTip("Browse loaded TIL (type library) types");
    layout->addWidget(standard_btn);

    layout->addSpacing(12);

    auto* cancel_btn = new QT::QPushButton("Cancel");
    cancel_btn->setDefault(true);
    layout->addWidget(cancel_btn);

    connect(manual_btn, &QT::QPushButton::clicked, this, &TypeSourceDialog::on_manual_clicked);
    connect(standard_btn, &QT::QPushButton::clicked, this, &TypeSourceDialog::on_standard_clicked);
    connect(cancel_btn, &QT::QPushButton::clicked, this, &TypeSourceDialog::on_cancel_clicked);
}

void TypeSourceDialog::on_manual_clicked() {
    result_ = Source::Manual;
    accept();
}

void TypeSourceDialog::on_standard_clicked() {
    result_ = Source::Standard;
    accept();
}

void TypeSourceDialog::on_local_clicked() {
    result_ = Source::Local;
    accept();
}

void TypeSourceDialog::on_cancel_clicked() {
    result_ = Source::Cancelled;
    reject();
}

TypeSourceDialog::Source TypeSourceDialog::show(QWidget* parent, uint64_t ea) {
    TypeSourceDialog dialog(parent, ea);
    dialog.exec();
    return dialog.result_;
}

// ============================================================================
// ManualTypeDialog
// ============================================================================

static const char* MANUAL_HINT =
    "Accepts any real-world format — annotations stripped automatically.\n\n"
    "Examples:\n"
    "  UINT WinExec(LPCSTR lpCmdLine, UINT uCmdShow);\n"
    "  typedef UINT (WINAPI *PWINEXEC)(LPCSTR, UINT);\n"
    "  UINT (__stdcall *)(LPCSTR, UINT)\n\n"
    "  NTSYSAPI\n"
    "  NTSTATUS\n"
    "  NTAPI\n"
    "  LdrGetProcedureAddress(\n"
    "      _In_     PVOID          DllHandle,\n"
    "      _In_opt_ PCANSI_STRING  ProcedureName,\n"
    "      _In_opt_ ULONG          ProcedureNumber,\n"
    "      _Out_    PVOID         *ProcedureAddress\n"
    "  );";

ManualTypeDialog::ManualTypeDialog(QWidget* parent, uint64_t ea, PreprocessFn preprocess_fn)
    : QT::QDialog(parent), preprocess_fn_(std::move(preprocess_fn)), ea_(ea) {
    setWindowTitle(QT::QString("ApplyCalleeTypeEx — Enter Prototype (%1)").arg(format_ea(ea)));
    setMinimumSize(640, 500);
    setModal(true);
    setup_ui();
}

ManualTypeDialog::~ManualTypeDialog() = default;

void ManualTypeDialog::setup_ui() {
    auto* root = new QT::QVBoxLayout(this);

    auto* splitter = new QT::QSplitter(QT::Qt::Vertical);
    root->addWidget(splitter, 1);

    // Top: Input editor
    auto* top = new QT::QWidget();
    auto* top_layout = new QT::QVBoxLayout(top);
    top_layout->setContentsMargins(0, 0, 0, 0);
    top_layout->addWidget(new QT::QLabel("Input:"));

    editor_ = new QT::QPlainTextEdit();
    editor_->setPlaceholderText(MANUAL_HINT);
    editor_->setLineWrapMode(QT::QPlainTextEdit::NoWrap);
    QFont fixed_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    editor_->setFont(fixed_font);
    top_layout->addWidget(editor_);

    splitter->addWidget(top);

    // Bottom: Preview
    auto* bot = new QT::QWidget();
    auto* bot_layout = new QT::QVBoxLayout(bot);
    bot_layout->setContentsMargins(0, 0, 0, 0);

    preview_label_ = new QT::QLabel("Preprocessed  (sent to parse_decl):");
    bot_layout->addWidget(preview_label_);

    preview_ = new QT::QPlainTextEdit();
    preview_->setReadOnly(true);
    preview_->setMaximumHeight(72);
    preview_->setFont(fixed_font);
    bot_layout->addWidget(preview_);

    splitter->addWidget(bot);
    splitter->setStretchFactor(0, 5);
    splitter->setStretchFactor(1, 1);

    // Bottom: Buttons
    auto* row = new QT::QHBoxLayout();
    row->addStretch();

    apply_btn_ = new QT::QPushButton("Apply");
    apply_btn_->setObjectName("Apply");
    apply_btn_->setEnabled(false);
    apply_btn_->setDefault(true);
    row->addWidget(apply_btn_);

    auto* cancel_btn = new QT::QPushButton("Cancel");
    row->addWidget(cancel_btn);

    root->addLayout(row);

    connect(editor_, &QT::QPlainTextEdit::textChanged, this, &ManualTypeDialog::on_text_changed);
    connect(apply_btn_, &QT::QPushButton::clicked, this, &ManualTypeDialog::on_apply_clicked);
    connect(cancel_btn, &QT::QPushButton::clicked, this, &ManualTypeDialog::on_cancel_clicked);
}

void ManualTypeDialog::on_text_changed() {
    update_preview();
}

void ManualTypeDialog::update_preview() {
    QT::QString text = editor_->toPlainText();
    QT::QString processed;

    if (!text.isEmpty() && text.length() < 10000) {
        std::string input = text.toStdString();
        try {
            auto result = preprocess_fn_(input);
            if (result && !result->empty()) {
                processed = QT::QString::fromLatin1(result->c_str(), static_cast<int>(result->size()));
            }
        } catch (...) {
            // Silently ignore preprocessing errors during typing
        }
    }

    preview_->setPlainText(processed);

    // Enable apply only if we have valid preprocessed text
    QT::QString trimmed = processed.trimmed();
    bool has_content = !trimmed.isEmpty();
    bool not_just_semicolon = (trimmed != ";");
    bool enable_apply = has_content && not_just_semicolon;
    
    if (apply_btn_) {
        apply_btn_->setEnabled(enable_apply);
    }
}

void ManualTypeDialog::on_apply_clicked() {
    original_text_ = editor_->toPlainText();
    preprocessed_text_ = preview_->toPlainText();
    accept();
}

void ManualTypeDialog::on_cancel_clicked() {
    reject();
}

std::optional<std::string> ManualTypeDialog::show(QWidget* parent, uint64_t ea, PreprocessFn preprocess_fn) {
    ManualTypeDialog dialog(parent, ea, std::move(preprocess_fn));
    if (dialog.exec() == QT::QDialog::Accepted) {
        return dialog.original_text().toStdString();
    }
    return std::nullopt;
}

// ============================================================================
// TilTypeDialog
// ============================================================================

TilTypeDialog::TilTypeDialog(QWidget* parent, uint64_t ea)
    : QT::QDialog(parent), ea_(ea) {
    setWindowTitle(QT::QString("ApplyCalleeTypeEx — Select TIL Type (%1)").arg(format_ea(ea)));
    setMinimumSize(700, 500);
    setModal(true);
    setup_ui();
}

TilTypeDialog::~TilTypeDialog() = default;

void TilTypeDialog::setup_ui() {
    auto* root = new QT::QVBoxLayout(this);

    auto* search_label = new QT::QLabel("Search:");
    root->addWidget(search_label);

    search_ = new QT::QLineEdit();
    search_->setPlaceholderText("Type to filter...");
    root->addWidget(search_);

    // Use table instead of list to show Type Name and Library columns
    table_ = new QT::QTableWidget();
    table_->setColumnCount(2);
    QStringList headers;
    headers << "Type Name" << "Library";
    table_->setHorizontalHeaderLabels(headers);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->setVisible(false);
    root->addWidget(table_);

    auto* row = new QT::QHBoxLayout();
    row->addStretch();

    auto* apply_btn = new QT::QPushButton("Apply");
    apply_btn->setEnabled(false);
    apply_btn->setDefault(true);
    row->addWidget(apply_btn);

    auto* cancel_btn = new QT::QPushButton("Cancel");
    row->addWidget(cancel_btn);

    root->addLayout(row);

    connect(search_, &QT::QLineEdit::textChanged, this, &TilTypeDialog::on_search_changed);
    connect(table_, &QT::QTableWidget::cellDoubleClicked, this, &TilTypeDialog::on_type_selected);
    connect(table_, &QT::QTableWidget::itemSelectionChanged, this, &TilTypeDialog::on_selection_changed);
    connect(apply_btn, &QT::QPushButton::clicked, this, &TilTypeDialog::on_apply_clicked);
    connect(cancel_btn, &QT::QPushButton::clicked, this, &TilTypeDialog::on_cancel_clicked);
}

namespace {

void populate_til_types_impl(QTableWidget* table) {
    table->setRowCount(0);

    // Use all_tils() + named_types_in() to iterate all types across all TILs
    auto tils_result = ida::type::all_tils();
    if (!tils_result) {
        table->setRowCount(1);
        auto* item = new QT::QTableWidgetItem("(TIL enumeration failed)");
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        table->setItem(0, 0, item);
        return;
    }

    // Collect all entries first
    std::vector<std::pair<std::string, std::string>> entries;  // {type_name, library_name}

    for (const auto& til_entry : *tils_result) {
        std::string library_name = std::string(til_entry.name);

        auto types_result = ida::type::named_types_in(til_entry.til, 0x0001 | 0x0002);  // NTF_FUNC | NTF_TYPE
        if (!types_result) continue;

        for (const auto& type_name : *types_result) {
            entries.emplace_back(std::string(type_name), library_name);
        }
    }

    if (entries.empty()) {
        table->setRowCount(1);
        auto* item = new QT::QTableWidgetItem("(No TIL types available)");
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        table->setItem(0, 0, item);
        return;
    }

    table->setRowCount(static_cast<int>(entries.size()));
    for (std::size_t i = 0; i < entries.size(); ++i) {
        auto* name_item = new QT::QTableWidgetItem(to_qstring(entries[i].first));
        auto* lib_item = new QT::QTableWidgetItem(to_qstring(entries[i].second));
        name_item->setData(Qt::UserRole, to_qstring(entries[i].first));
        table->setItem(static_cast<int>(i), 0, name_item);
        table->setItem(static_cast<int>(i), 1, lib_item);
    }
}

} // anonymous namespace

void TilTypeDialog::populate_types() {
    populate_til_types_impl(table_);
}

void TilTypeDialog::on_search_changed(const QString& text) {
    for (int i = 0; i < table_->rowCount(); ++i) {
        auto* name_item = table_->item(i, 0);
        bool matches = name_item && name_item->text().contains(text, Qt::CaseInsensitive);
        table_->setRowHidden(i, !matches);
    }
}

void TilTypeDialog::on_type_selected(int row, int column) {
    Q_UNUSED(column);
    auto* item = table_->item(row, 0);
    if (item) {
        result_name_ = item->data(Qt::UserRole).toString();
        accept();
    }
}

void TilTypeDialog::on_selection_changed() {
    auto* apply_btn = findChild<QT::QPushButton*>("Apply");
    if (apply_btn) {
        apply_btn->setEnabled(table_->currentRow() >= 0);
    }
}

void TilTypeDialog::on_apply_clicked() {
    int row = table_->currentRow();
    if (row >= 0) {
        auto* item = table_->item(row, 0);
        if (item) {
            result_name_ = item->data(Qt::UserRole).toString();
            accept();
        }
    }
}

void TilTypeDialog::on_cancel_clicked() {
    reject();
}

std::optional<std::string> TilTypeDialog::show(QWidget* parent, uint64_t ea) {
    TilTypeDialog dialog(parent, ea);
    dialog.populate_types();
    dialog.exec();
    return dialog.result_name().isEmpty() ? std::nullopt : std::make_optional(dialog.result_name().toStdString());
}

// ============================================================================
// LocalTypeDialog
// ============================================================================

LocalTypeDialog::LocalTypeDialog(QWidget* parent, uint64_t ea)
    : QT::QDialog(parent), ea_(ea) {
    setWindowTitle(QT::QString("ApplyCalleeTypeEx — Select Local Type (%1)").arg(format_ea(ea)));
    setMinimumSize(500, 400);
    setModal(true);
    setup_ui();
}

LocalTypeDialog::~LocalTypeDialog() = default;

void LocalTypeDialog::setup_ui() {
    auto* root = new QT::QVBoxLayout(this);

    auto* search_label = new QT::QLabel("Search:");
    root->addWidget(search_label);

    search_ = new QT::QLineEdit();
    search_->setPlaceholderText("Type to filter...");
    root->addWidget(search_);

    list_ = new QT::QListWidget();
    root->addWidget(list_);

    auto* row = new QT::QHBoxLayout();
    row->addStretch();

    auto* apply_btn = new QT::QPushButton("Apply");
    apply_btn->setEnabled(false);
    apply_btn->setDefault(true);
    row->addWidget(apply_btn);

    auto* cancel_btn = new QT::QPushButton("Cancel");
    row->addWidget(cancel_btn);

    root->addLayout(row);

    connect(search_, &QT::QLineEdit::textChanged, this, &LocalTypeDialog::on_search_changed);
    connect(list_, &QT::QListWidget::itemDoubleClicked, this, &LocalTypeDialog::on_type_selected);
    connect(list_, &QT::QListWidget::itemSelectionChanged, this, &LocalTypeDialog::on_selection_changed);
    connect(apply_btn, &QT::QPushButton::clicked, this, &LocalTypeDialog::on_apply_clicked);
    connect(cancel_btn, &QT::QPushButton::clicked, this, &LocalTypeDialog::on_cancel_clicked);
}

void LocalTypeDialog::populate_types() {
    populate_local_types(list_);
}

void LocalTypeDialog::on_search_changed(const QString& text) {
    for (int i = 0; i < list_->count(); ++i) {
        QListWidgetItem* item = list_->item(i);
        item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
    }
}

void LocalTypeDialog::on_type_selected(QListWidgetItem* item) {
    result_name_ = item->text();
    accept();
}

void LocalTypeDialog::on_selection_changed() {
    auto* apply_btn = findChild<QT::QPushButton*>("Apply");
    if (apply_btn) {
        apply_btn->setEnabled(list_->currentItem() != nullptr);
    }
}

void LocalTypeDialog::on_apply_clicked() {
    auto* item = list_->currentItem();
    if (item) {
        result_name_ = item->text();
        accept();
    }
}

void LocalTypeDialog::on_cancel_clicked() {
    reject();
}

std::optional<std::string> LocalTypeDialog::show(QWidget* parent, uint64_t ea) {
    LocalTypeDialog dialog(parent, ea);
    dialog.populate_types();
    dialog.exec();
    return dialog.result_name().isEmpty() ? std::nullopt : std::make_optional(dialog.result_name().toStdString());
}