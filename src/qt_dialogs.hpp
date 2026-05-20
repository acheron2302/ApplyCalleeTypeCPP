// qt_dialogs.hpp
// Qt dialogs for ApplyCalleTypeCpp plugin
// These dialogs are optional - the plugin will use IDA forms if Qt is not available

#ifndef QT_DIALOGS_HPP
#define QT_DIALOGS_HPP

// Include Qt headers - IDA SDK Qt uses QT_NAMESPACE=QT
// but types should also be available at global scope
#include <QDialog>
#include <QString>
#include <QPlainTextEdit>
#include <QLabel>
#include <QWidget>
#include <QFontDatabase>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QListWidgetItem>

#ifdef QT_NAMESPACE
QT_USE_NAMESPACE
#endif

#include <functional>
#include <optional>
#include <string>
#include <cstdint>

void populate_til_types(QListWidget* list);
void populate_til_types(QTableWidget* table);
void populate_local_types(QListWidget* list);

class TypeSourceDialog : public QDialog {
    Q_OBJECT

public:
    enum class Source { Manual, Standard, Local, Cancelled };

    explicit TypeSourceDialog(QWidget* parent, uint64_t ea);
    ~TypeSourceDialog() override;

    static Source show(QWidget* parent, uint64_t ea);

private:
    void setup_ui();
    void on_manual_clicked();
    void on_standard_clicked();
    void on_local_clicked();
    void on_cancel_clicked();

    Source result_ = Source::Cancelled;
    uint64_t ea_ = 0;
};

class ManualTypeDialog : public QDialog {
    Q_OBJECT

public:
    using PreprocessFn = std::function<std::optional<std::string>(std::string_view)>;

    explicit ManualTypeDialog(QWidget* parent, uint64_t ea, PreprocessFn preprocess_fn);
    ~ManualTypeDialog() override;

    static std::optional<std::string> show(QWidget* parent, uint64_t ea, PreprocessFn preprocess_fn);

    QString original_text() const { return original_text_; }

private slots:
    void on_text_changed();
    void on_apply_clicked();
    void on_cancel_clicked();

private:
    void setup_ui();
    void update_preview();

    QPlainTextEdit* editor_ = nullptr;
    QPlainTextEdit* preview_ = nullptr;
    QLabel* preview_label_ = nullptr;
    QPushButton* apply_btn_ = nullptr;
    QString original_text_;
    QString preprocessed_text_;
    PreprocessFn preprocess_fn_;
    uint64_t ea_ = 0;
};

class TilTypeDialog : public QDialog {
    Q_OBJECT

public:
    explicit TilTypeDialog(QWidget* parent, uint64_t ea);
    ~TilTypeDialog() override;

    static std::optional<std::string> show(QWidget* parent, uint64_t ea);

    QString result_name() const { return result_name_; }

private slots:
    void on_search_changed(const QString& text);
    void on_type_selected(int row, int column);
    void on_selection_changed();
    void on_apply_clicked();
    void on_cancel_clicked();

private:
    void setup_ui();
    void populate_types();

    QTableWidget* table_ = nullptr;
    QLineEdit* search_ = nullptr;
    QPushButton* apply_btn_ = nullptr;
    QString result_name_;
    uint64_t ea_ = 0;
};

class LocalTypeDialog : public QDialog {
    Q_OBJECT

public:
    explicit LocalTypeDialog(QWidget* parent, uint64_t ea);
    ~LocalTypeDialog() override;

    static std::optional<std::string> show(QWidget* parent, uint64_t ea);

    QString result_name() const { return result_name_; }

private slots:
    void on_search_changed(const QString& text);
    void on_type_selected(QListWidgetItem* item);
    void on_selection_changed();
    void on_apply_clicked();
    void on_cancel_clicked();

private:
    void setup_ui();
    void populate_types();

    QListWidget* list_ = nullptr;
    QLineEdit* search_ = nullptr;
    QPushButton* apply_btn_ = nullptr;
    QString result_name_;
    uint64_t ea_ = 0;
};

#endif // QT_DIALOGS_HPP