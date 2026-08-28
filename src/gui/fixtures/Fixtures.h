#pragma once

#include <imgui.h>

#include <span>

namespace wgrd::gui::fixtures {

struct CatalogRow {
    const char* mark;
    const char* name;
    const char* source;
    const char* release;
    const char* size;
    const char* kind;
    const char* build;
    const char* state;
    const char* action;
    bool queued;
};

struct OrderRow {
    const char* index;
    const char* name;
    const char* folder;
    const char* kind;
    bool enabled;
    const char* warningTag;
    const char* warningText;
    bool blocking;
};

struct TransferRow {
    const char* transport;
    const char* name;
    const char* version;
    const char* rate;
    const char* eta;
    const char* stats;
    const char* verify;
    float local;
    float fetched;
    float active;
};

struct ProfileRow {
    const char* name;
    const char* badge;
    const char* meta;
    const char* primaryAction;
};

struct AttentionItem {
    const char* title;
    const char* body;
    const char* primary;
    const char* secondary;
    bool blocking;
};

struct FileCheckRow {
    const char* path;
    const char* size;
    const char* state;
    bool present;
};

std::span<const CatalogRow> Catalog();
std::span<const OrderRow> Order();
std::span<const TransferRow> Transfers();
std::span<const ProfileRow> Profiles();
std::span<const AttentionItem> Attention();
std::span<const FileCheckRow> FileChecks();

}
