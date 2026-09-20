// The one translation unit in the project compiled from the standard headers
// rather than from `import std;`.
//
// MSVC's std module does not carry the definitions of a few variables that its
// own machinery references once it is instantiated in a translation unit that
// only imports the module, so importing std and comparing two strings or
// formatting a floating point number fails to link:
//
//   error LNK2019: unresolved external symbol
//     "public: static int const std::_General_precision_tables_2<double>::_Max_P"
//     referenced in function std::_Floating_to_chars_general_precision<double>
//   error LNK2019: unresolved external symbol
//     "public: static struct std::strong_ordering const std::strong_ordering::less"
//     referenced in function std::operator<=>(const std::string&, const std::string&)
//
// A translation unit that includes the headers does have those definitions, so
// this file includes them and takes each address. The definitions land in this
// object file and the link succeeds. Nothing else belongs here, and the whole
// file goes away once MSVC's std module defines what it references.
#ifdef _MSC_VER

#include <charconv>
#include <compare>

namespace hui::std_module_anchors {

// External linkage and non-const pointers on purpose: an unused `const` object
// at namespace scope has internal linkage, and the compiler would be free to
// discard it along with the definitions it was meant to pull in.
extern const int* const general_precision_double = &std::_General_precision_tables_2<double>::_Max_P;
extern const int* const general_precision_float = &std::_General_precision_tables_2<float>::_Max_P;
extern const std::strong_ordering* const ordering_less = &std::strong_ordering::less;
extern const std::strong_ordering* const ordering_equal = &std::strong_ordering::equal;
extern const std::strong_ordering* const ordering_greater = &std::strong_ordering::greater;

}  // namespace hui::std_module_anchors

#endif
