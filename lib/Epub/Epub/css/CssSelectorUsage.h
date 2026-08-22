#pragma once

#include <string>
#include <string_view>
#include <vector>

// Collects the tag and class names a chapter actually uses.  CssParser only
// resolves tag, .class and tag.class selectors, so all other cached rules can
// stay on SD instead of occupying heap during section construction.
class CssSelectorUsage {
 public:
  bool scanHtmlFile(const std::string& path);
  [[nodiscard]] bool matches(const std::string& selectorKey) const;

 private:
  static constexpr size_t MAX_TAGS = 64;
  static constexpr size_t MAX_CLASSES = 256;
  static constexpr size_t MAX_TOKEN_LENGTH = 128;

  void addTag(std::string_view name);
  void addClass(std::string_view name);
  [[nodiscard]] bool containsTag(std::string_view name) const { return contains(tags_, name); }
  [[nodiscard]] bool containsClass(std::string_view name) const { return contains(classes_, name); }
  static bool contains(const std::vector<std::string>& list, std::string_view name);

  std::vector<std::string> tags_;
  std::vector<std::string> classes_;
  bool overflowed_ = false;
};
