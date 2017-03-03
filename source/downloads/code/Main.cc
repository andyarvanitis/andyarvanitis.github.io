#include <cstring>
#include "Main.hh"

namespace Main {
 
  static auto bitor_all( const any::array& opts ) -> int {
    int combined_opts = 0;
    for (auto it = opts.cbegin(), end = opts.cend(); it != end; it++) {
      combined_opts |= static_cast<int>(*it);
    }
    return combined_opts;
  }

  auto compile( const char * pattern, const any::array& options ) -> any {
    const char * err = nullptr;
    int erroffset = 0;
    auto ptr = pcre_compile(pattern,
                            bitor_all(options),
                            &err,
                            &erroffset,
                            nullptr);
    return ptr ? Right(managed<pcre>(ptr, pcre_free)) : Left(err);
  }

  auto capturedCount( const any& code ) -> int {
    int count = 0;
    pcre_fullinfo(&cast<pcre>(code),
                  nullptr,
                  PCRE_INFO_CAPTURECOUNT,
                  &count);
    return count;    
  }

  auto exec( const any& code,
             const char * subject,
             int startoffset,
             const any::array& options,
             int ovecsize ) -> any {
    int ovector[ovecsize];
    auto r = pcre_exec(&cast<pcre>(code),
                       nullptr,
                       subject,
                       strlen(subject),
                       startoffset,
                       bitor_all(options),
                       ovector,
                       ovecsize);
    any::array offsets;
    for (auto i = 0; i < r * 2; i += 2) {
      offsets.push_back( Tuple(ovector[i], ovector[i+1]) );
    }
    return r < 0 ? Left(r) : Right(offsets);
  }

} // Main
