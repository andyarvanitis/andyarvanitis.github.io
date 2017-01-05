#ifndef MainFFI_HH
#define MainFFI_HH

#include <pcre.h>
#include "PureScript/PureScript.hh"

namespace Main {

  using namespace PureScript;

  enum {
    caseless = PCRE_CASELESS,
    dotall   = PCRE_DOTALL
  };

  auto compile( const char * pattern, const any::array& options ) -> any;

  auto capturedCount( const any& code ) -> int;

  auto exec( const any& code,
             const char * subject,
             int startoffset,
             const any::array& options,
             int ovecsize ) -> any;

} // namespace Main

#endif // MainFFI_HH
