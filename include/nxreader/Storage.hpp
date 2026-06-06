#pragma once

namespace nxreader {

void ensureSaveDirExists();
int loadLastPage(const char* bookPath, int maxPage);
void saveLastPage(const char* bookPath, int page);

}  // namespace nxreader
