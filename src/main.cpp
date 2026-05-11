// idax_apply_callee_type_ex.cpp
// Port of Mandiant FLARE ApplyCalleeTypeEx plugin to idax (C++23)
// Applies function prototypes to indirect CALL instructions
// Compatible with IDA Pro 8.x through 9.3+
//
// Build: Compile as idax plugin with idax::idax linked

#include <ida/idax.hpp>
#include <ida/ui.hpp>
#include <ida/type.hpp>

#include <QApplication>

#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <fmt/format.h>

#include "preprocessing.hpp"
#include "qt_dialogs.hpp"

// -------------------- Constants --------------------

namespace constants {
constexpr std::string_view PLUGIN_NAME = "ApplyCalleeTypeEx";
constexpr std::string_view PLUGIN_COMMENT =
  "Apply callee type to indirect call location";
constexpr std::string_view PLUGIN_HELP =
  "Place cursor on an indirect CALL and press Shift+A.";
constexpr std::string_view ACTION_NAME = "dump-guy:apply_callee_type_ex";
constexpr std::string_view ACTION_LABEL = "ApplyCalleeTypeEx";
constexpr std::string_view ACTION_HOTKEY = "Shift+A";
constexpr std::string_view MENU_PATH = "Edit/Operand type/";
}

// -------------------- Logging --------------------

static void log_message(const std::string& msg) {
  ida::ui::message("[ApplyCalleeTypeEx] " + msg + "\n");
}

// -------------------- Type Parsing --------------------

class TypeParser {
public:
  static ida::Result<ida::type::TypeInfo> parse_from_string(std::string_view type_str) {
    if (type_str.empty()) {
      return std::unexpected(ida::Error{
        ida::ErrorCategory::Validation, 0, "Empty type string", ""
      });
    }

    auto cleaned_result = preprocessing::preprocess_prototype(type_str);
    if (!cleaned_result) {
      log_message("Preprocessing failed for input");
      return std::unexpected(ida::Error{
        ida::ErrorCategory::Validation, 0, "Preprocessing failed", ""
      });
    }

    std::string cleaned = std::move(*cleaned_result);
    if (cleaned.empty() || cleaned == ";") {
      log_message("Preprocessing resulted in empty declaration");
      return std::unexpected(ida::Error{
        ida::ErrorCategory::Validation, 0, "Preprocessing resulted in empty declaration", ""
      });
    }

    log_message("Preprocessed: " + cleaned);
    return parse_preprocessed(cleaned);
  }

  static ida::Result<ida::type::TypeInfo> parse_preprocessed(const std::string& cleaned) {
    if (cleaned.empty()) {
      return std::unexpected(ida::Error{
        ida::ErrorCategory::Validation, 0, "Empty declaration", ""
      });
    }

    // Basic validation - ensure it looks like a type declaration
    if (cleaned.find('(') == std::string::npos) {
      return std::unexpected(ida::Error{
        ida::ErrorCategory::Validation, 0, "Declaration does not contain a function signature", ""
      });
    }

    // Truncate very long declarations that might overwhelm IDA's parser
    std::string truncated = cleaned;
    if (truncated.size() > 4096) {
      truncated = truncated.substr(0, 4096);
    }

    // Log the preprocessed string so we can see what we're trying to parse
    log_message("Parsing: " + truncated);

    ida::Result<ida::type::TypeInfo> result;
    try {
      result = ida::type::TypeInfo::from_declaration(truncated);
    } catch (const std::exception& e) {
      log_message(std::string("Exception: ") + e.what());
      return std::unexpected(ida::Error{
        ida::ErrorCategory::Validation, 0,
        "Type parsing failed: " + std::string(e.what()), ""
      });
    } catch (...) {
      log_message("Unknown exception during type parsing");
      return std::unexpected(ida::Error{
        ida::ErrorCategory::Validation, 0, "Type parsing failed with unknown error", ""
      });
    }
    
    if (!result) {
      return std::unexpected(ida::Error{
        ida::ErrorCategory::Validation, 0,
        "Could not parse type declaration", ""
      });
    }

    if (!result->is_function() && !result->is_pointer()) {
      return std::unexpected(ida::Error{
        ida::ErrorCategory::Validation, 0, "Parsed type is not a function or function pointer", ""
      });
    }

    return result;
  }
};

// -------------------- Type Selection --------------------

static QWidget* get_qt_parent() {
    return QApplication::activeWindow();
}

class TypeSelector {
public:
  static ida::Result<ida::type::TypeInfo> get_type_from_user(ida::Address ea) {
    auto source = show_source_dialog(ea);

    switch (source) {
      case TypeSourceDialog::Source::Manual:
        return get_manual_type(ea);
      case TypeSourceDialog::Source::Standard:
        return choose_standard_type(ea);
      case TypeSourceDialog::Source::Local:
        return choose_local_type(ea);
      default:
        return std::unexpected(ida::Error{
          ida::ErrorCategory::Validation, 0, "User cancelled", ""
        });
    }
  }

  private:
  static TypeSourceDialog::Source show_source_dialog(ida::Address ea) {
    return TypeSourceDialog::show(get_qt_parent(), ea);
  }

  static ida::Result<ida::type::TypeInfo> get_manual_type(ida::Address ea) {
    auto result_text = ManualTypeDialog::show(
        get_qt_parent(), ea, preprocessing::preprocess_prototype);
    if (!result_text) {
      return std::unexpected(ida::Error{
        ida::ErrorCategory::Validation, 0, "No input provided", ""
      });
    }

    auto result = TypeParser::parse_from_string(*result_text);
    if (!result) {
      log_message("Could not parse prototype - check syntax and try again.");
    }
    return result;
  }

  static ida::Result<ida::type::TypeInfo> choose_standard_type(ida::Address ea) {
    auto name = TilTypeDialog::show(get_qt_parent(), ea);
    if (!name) {
      return std::unexpected(ida::Error{
        ida::ErrorCategory::Validation, 0, "No type selected", ""
      });
    }

    auto tif = ida::type::ensure_named_type(*name);
    if (!tif) {
      std::string warn = fmt::format("Type '{}' not found in TIL", *name);
      ida::ui::warning(warn.c_str());
      return std::unexpected(ida::Error{
        ida::ErrorCategory::NotFound, 0, warn, ""
      });
    }

    return tif;
  }

  static ida::Result<ida::type::TypeInfo> choose_local_type(ida::Address ea) {
    auto name = LocalTypeDialog::show(get_qt_parent(), ea);
    if (!name) {
      return std::unexpected(ida::Error{
        ida::ErrorCategory::Validation, 0, "No type selected", ""
      });
    }

    auto tif = ida::type::TypeInfo::by_name(*name);
    if (!tif) {
      return std::unexpected(ida::Error{
        ida::ErrorCategory::NotFound, 0,
        fmt::format("Could not retrieve type '{}'", *name), ""
      });
    }

    return tif;
  }
};

// -------------------- Core Apply Logic --------------------

class CalleeTypeApplier {
public:
  static bool apply_type_to_call(ida::Address ea, const ida::type::TypeInfo& tif) {
    auto insn = ida::instruction::decode(ea);
    if (!insn) {
      log_message(fmt::format("No instruction at 0x{:X}.", ea));
      return false;
    }

    if (insn->mnemonic() != "call") {
      log_message(fmt::format("0x{:X} is not a CALL instruction (mnemonic: {}).",
                              ea, insn->mnemonic()));
      log_message(fmt::format("Error the instruction at 0x{:X} is {}",
                              ea,
                              ida::instruction::text(ea).emplace()));
      return false;
    }

    auto op0 = insn->operand(0);
    if (!op0) {
      log_message(fmt::format("Could not get operand 0 at 0x{:X}.", ea));
      return false;
    }

    auto op_type = op0->type();
    if (op_type == ida::instruction::OperandType::NearAddress ||
        op_type == ida::instruction::OperandType::FarAddress) {
      log_message(fmt::format("Direct CALL at 0x{:X} - IDA resolves direct calls automatically.", ea));
      return false;
    }

    ida::type::TypeInfo apply_tif = tif;
    if (tif.is_function()) {
      apply_tif = ida::type::TypeInfo::pointer_to(tif);
    }

    auto apply_result = apply_tif.apply(ea);
    if (!apply_result) {
      
      log_message(fmt::format("apply() failed at 0x{:X} with error: {}.",
                              ea,
                              apply_result.error().message));
      return false;
    }

    return true;
  }
};

// -------------------- Action Handler --------------------

class ApplyCalleeTypeActionHandler {
public:
  static ida::Status handle_activation(const ida::plugin::ActionContext& ctx) {
    ida::Address target_ea = ctx.current_address;

    if (target_ea == ida::BadAddress) {
      auto screen = ida::ui::screen_address();
      if (screen) target_ea = *screen;
    }

    if (target_ea == ida::BadAddress) {
      log_message("No address selected.");
      return ida::ok();
    }

    auto tif = TypeSelector::get_type_from_user(target_ea);
    if (!tif) {
      return ida::ok();
    }

    auto type_str = tif->to_string();
    std::string type_name = type_str ? *type_str : "unknown";

    if (CalleeTypeApplier::apply_type_to_call(target_ea, *tif)) {
      log_message(fmt::format("Applied '{}' at 0x{:X}.", type_name, target_ea));
    } else {
      log_message(fmt::format("Failed to apply '{}' at 0x{:X}.", type_name, target_ea));
    }

    return ida::ok();
  }

  static bool is_enabled(const ida::plugin::ActionContext& ctx) {
    ida::ui::WidgetType wtype = ida::ui::WidgetType::Unknown;
    if (ctx.widget_handle) {
      wtype = ida::ui::widget_type(ctx.widget_handle);
    }
    return wtype == ida::ui::WidgetType::Disassembly ||
           wtype == ida::ui::WidgetType::Pseudocode;
  }
};

// -------------------- UI Hooks --------------------

class ApplyCalleeTypeHooks {
public:
  ApplyCalleeTypeHooks() : popup_token_(0) {}

  void hook() {
    auto result = ida::ui::on_popup_ready([this](const ida::ui::PopupEvent& ev) {
      ida::ui::WidgetType wtype = ida::ui::widget_type(ev.widget);
      if (wtype == ida::ui::WidgetType::Disassembly ||
          wtype == ida::ui::WidgetType::Pseudocode) {
        auto status = ida::ui::attach_dynamic_action(
          ev.popup,
          ev.widget,
          std::string(constants::ACTION_NAME),
          std::string(constants::ACTION_LABEL),
          nullptr,
          "applycalleetypeex/",
          -1);
      }
    });
    if (result) {
      popup_token_ = *result;
    }
  }

  void unhook() {
    if (popup_token_ != 0) {
      (void)ida::ui::unsubscribe(popup_token_);
      popup_token_ = 0;
    }
  }

private:
  ida::ui::Token popup_token_;
};

// -------------------- Plugin --------------------

class ApplyCalleeTypePlugin : public ida::plugin::Plugin {
public:
  [[nodiscard]]
  ida::plugin::Info info() const override {
    return {
      std::string(constants::PLUGIN_NAME),
      "Shift+A",
      std::string(constants::PLUGIN_COMMENT),
      std::string(constants::PLUGIN_HELP)
    };
  }

  bool init() override {
    try {
      ida::plugin::Action action;
      action.id = std::string(constants::ACTION_NAME);
      action.label = std::string(constants::ACTION_LABEL);
      action.hotkey = std::string(constants::ACTION_HOTKEY);
      action.tooltip = std::string(constants::PLUGIN_HELP);
      action.handler_with_context = [](const ida::plugin::ActionContext& ctx) {
        return ApplyCalleeTypeActionHandler::handle_activation(ctx);
      };
      action.enabled_with_context = [](const ida::plugin::ActionContext& ctx) {
        return ApplyCalleeTypeActionHandler::is_enabled(ctx);
      };

      auto reg_result = ida::plugin::register_action(action);
      if (!reg_result) {
        ida::ui::warning("Failed to register action");
        return false;
      }

      auto menu_result = ida::plugin::attach_to_menu(
        std::string(constants::MENU_PATH),
        std::string(constants::ACTION_NAME));
      if (!menu_result) {
        ida::ui::warning("Failed to attach to menu");
      }

      hooks_ = std::make_unique<ApplyCalleeTypeHooks>();
      hooks_->hook();

      ida::ui::message("ApplyCalleeTypeEx ready\n");
      return true;
    } catch (const std::exception& e) {
      ida::ui::warning(fmt::format("Plugin init failed: {}", e.what()));
      return false;
    } catch (...) {
      ida::ui::warning("Plugin init failed: unknown error");
      return false;
    }
  }

  void term() override {
    if (hooks_) {
      hooks_->unhook();
      hooks_.reset();
    }
    (void)ida::plugin::detach_from_menu(std::string(constants::MENU_PATH),
                                   std::string(constants::ACTION_NAME));
    (void)ida::plugin::unregister_action(std::string(constants::ACTION_NAME));
  }

  ida::Status run(size_t) override {
    log_message("Place cursor on an indirect CALL and press Shift+A.");
    return ida::ok();
  }

private:
  std::unique_ptr<ApplyCalleeTypeHooks> hooks_;
};

IDAX_PLUGIN(ApplyCalleeTypePlugin)