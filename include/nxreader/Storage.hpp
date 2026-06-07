#pragma once

namespace nxreader {

struct AppSettings;

void ensureSaveDirExists();
int loadLastPage(const char* bookPath, int maxPage);
void saveLastPage(const char* bookPath, int page);
AppSettings loadSettings();
void saveSettings(const AppSettings& settings);

}  // namespace nxreader
