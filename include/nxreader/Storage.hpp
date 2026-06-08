#pragma once

#include <string>
#include <vector>

namespace nxreader {

struct AppSettings;
struct Annotation;

void ensureSaveDirExists();
int loadLastPage(const char* bookPath, int maxPage);
void saveLastPage(const char* bookPath, int page);
AppSettings loadSettings();
void saveSettings(const AppSettings& settings);
std::vector<Annotation> loadAnnotations(const char* bookPath);
void saveAnnotations(const char* bookPath, const std::vector<Annotation>& annotations);

}  // namespace nxreader
