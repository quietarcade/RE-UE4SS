#ifdef __linux__

#include <cstdio>
#include <fstream>
#include <filesystem>

#include <File/FileType/LinuxFile.hpp>

namespace RC::File
{
    auto LinuxFile::create_all_directories(const std::filesystem::path& file_name_and_path) -> void
    {
        std::filesystem::create_directories(file_name_and_path.parent_path());
    }

    auto LinuxFile::close_file() -> void
    {
        if (m_file)
        {
            fclose(static_cast<FILE*>(m_file));
            m_file = nullptr;
        }
        m_is_file_open = false;
    }

    auto LinuxFile::is_file_open() const -> bool { return m_is_file_open; }
    auto LinuxFile::set_file(HANDLE new_file) -> void { m_file = new_file; }
    auto LinuxFile::set_is_file_open(bool new_is_open) -> void { m_is_file_open = new_is_open; }
    auto LinuxFile::get_file() -> HANDLE { return m_file; }
    auto LinuxFile::serialization_file_exists() -> bool { return std::filesystem::exists(m_serialization_file_path_and_name); }

    auto LinuxFile::is_valid() noexcept -> bool { return m_is_file_open && m_file != nullptr; }
    auto LinuxFile::invalidate_file() noexcept -> void { m_file = nullptr; m_is_file_open = false; }

    auto LinuxFile::delete_file(const std::filesystem::path& path) -> void
    {
        std::filesystem::remove(path);
    }

    auto LinuxFile::delete_file() -> void
    {
        close_file();
        std::filesystem::remove(m_file_path_and_name);
    }

    auto LinuxFile::get_raw_handle() noexcept -> void* { return m_file; }
    auto LinuxFile::get_file_path() const noexcept -> const std::filesystem::path& { return m_file_path_and_name; }

    auto LinuxFile::set_serialization_output_file(const std::filesystem::path& output_file) noexcept -> void
    {
        m_serialization_file_path_and_name = output_file;
    }

    auto LinuxFile::serialize_identifying_properties() -> void { /* no-op on Linux */ }
    auto LinuxFile::deserialize_identifying_properties() -> void { /* no-op on Linux */ }
    auto LinuxFile::is_deserialized_and_live_equal() -> bool { return false; }
    auto LinuxFile::invalidate_serialization() -> void { /* no-op on Linux */ }

    auto LinuxFile::serialize_item(const GenericItemData& data, bool is_internal_item) -> void { /* no-op */ }
    auto LinuxFile::get_serialized_item(size_t data_size, bool is_internal_item) -> void* { return nullptr; }

    auto LinuxFile::close_current_file() -> void { close_file(); }

    auto LinuxFile::write_string_to_file(StringViewType string_to_write) -> void
    {
        if (!m_file) return;
        FILE* f = static_cast<FILE*>(m_file);
        // StringViewType is std::wstring_view on Linux
        // Write as UTF-8
        std::string narrow;
        narrow.reserve(string_to_write.size());
        for (auto wc : string_to_write)
        {
            if (wc < 0x80) narrow.push_back(static_cast<char>(wc));
            else narrow.push_back('?');
        }
        fwrite(narrow.data(), 1, narrow.size(), f);
        fflush(f);
    }

    auto LinuxFile::is_same_as(LinuxFile& other_file) -> bool
    {
        return m_file_path_and_name == other_file.m_file_path_and_name;
    }

    auto LinuxFile::read_all() const -> StringType
    {
        std::ifstream file(m_file_path_and_name, std::ios::binary);
        if (!file) return {};
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        StringType result;
        result.reserve(content.size());
        for (char c : content) result.push_back(static_cast<wchar_t>(static_cast<unsigned char>(c)));
        return result;
    }

    auto LinuxFile::memory_map() -> std::span<uint8_t> { return {}; }

    auto LinuxFile::open_file(const std::filesystem::path& file_name_and_path, const OpenProperties& open_properties) -> LinuxFile
    {
        LinuxFile linux_file{};
        linux_file.m_file_path_and_name = file_name_and_path;
        linux_file.m_open_properties = open_properties;

        if (open_properties.create_if_non_existent == CreateIfNonExistent::Yes)
        {
            create_all_directories(file_name_and_path);
        }

        const char* mode = "r";
        if (open_properties.open_for == OpenFor::Writing)
        {
            if (open_properties.overwrite_existing_file == OverwriteExistingFile::Yes)
                mode = "w";
            else
                mode = "a";
        }
        else if (open_properties.open_for == OpenFor::ReadWrite)
        {
            mode = "r+";
        }

        FILE* f = fopen(file_name_and_path.c_str(), mode);
        if (!f && open_properties.create_if_non_existent == CreateIfNonExistent::Yes)
        {
            f = fopen(file_name_and_path.c_str(), "w+");
        }

        linux_file.m_file = f;
        linux_file.m_is_file_open = (f != nullptr);

        return linux_file;
    }
} // namespace RC::File

#endif // __linux__
