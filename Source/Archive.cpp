#include "Archive.h"
#include "DynamicArray.h"
static Memory_Arena loading_arena;

void Archive::create(const char* archive_path)
{
	loading_arena.init("loading", 8'000'000);

	path = archive_path;
	file_handle = std::fopen(archive_path, "rb");

	char magic[4];

	std::fread(magic, 1, 4, file_handle);
	if (magic[0] != 'A' || magic[1] != 'B' || magic[2] != 'C' || magic[3] != 'D') {
		printf("Bad magic\n");
		std::fclose(file_handle);
		return;
	}

	uint32_t version;
	std::fread(&version, 1, 4, file_handle);
	printf("Archive version %d\n", version);

	uint32_t num_entries;
	std::fread(&num_entries, 1, 4, file_handle);
	uint32_t data_offset;
	std::fread(&data_offset, 1, 4, file_handle);

	uint32_t string_offset = 16 + num_entries * 20;
	uint32_t string_table_len = data_offset - string_offset;
	string_table.resize(string_table_len);
	std::fseek(file_handle,string_offset,SEEK_SET);
	std::fread(string_table.data(), 1, string_table.size(), file_handle);

	std::fseek(file_handle, 16, SEEK_SET);
	entries.resize(num_entries);
	std::fread(entries.data(), 1, num_entries * sizeof(Archive_Entry), file_handle);

	for (int i = 0; i < num_entries; i++) {
		Archive_Entry& e = entries[i];
		StringUtils::StringHash hash(&string_table.at(e.string_offset));
		if (hash_to_index.find(hash.computedHash) != hash_to_index.end()) {
			printf("Collision! %s\n", &string_table.at(e.string_offset));
		}
		hash_to_index[hash.computedHash] = i;
	}
}