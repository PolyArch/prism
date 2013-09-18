#ifndef ELFPARSE_HH
#define ELFPARSE_HH

#include <string>
#include <ostream>
#include <sstream>
#include <iomanip>
#include <elfio/elfio.hpp>
#include <iostream>
#include <elfio/elfio_dump.hpp>
#include <map>

#include <boost/serialization/set.hpp>
#include <boost/serialization/bitset.hpp>


#ifdef _MSC_VER
  #define _SCL_SECURE_NO_WARNINGS
  #define ELFIO_NO_INTTYPES
#endif

//for demangle
#if defined(__GLIBCXX__) || defined(__GLIBCPP__)
#include <cxxabi.h>
#endif



using namespace ELFIO;

#define FUNC_TYPE 2
class prof_symbol {
public:
  std::string   name;
  Elf64_Addr    value;
  Elf_Xword     size;
  unsigned char bind;
  unsigned char type;
  Elf_Half      section;
  unsigned char other;

  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & name;
    ar & value;
    ar & size;
    ar & bind;
    ar & type;
    ar & section;
    ar & other;
  }


  bool isFunc() {return type==FUNC_TYPE;}
};

typedef std::map<uint64_t,prof_symbol> SYM_TAB;


class ELF_parser {
public:
  #if defined(__GLIBCXX__) || defined(__GLIBCPP__)
  static inline
  std::string
  demangle(const char* name)
  {
      // need to demangle C++ symbols
      char*       realname;
      std::size_t len; 
      int         stat;
       
      realname = abi::__cxa_demangle(name,NULL,&len,&stat);
      if (realname != NULL)
      {
          std::string   out(realname);
          std::free(realname);
          return out;
      }
      
      return std::string(name);
  }
  
  #else
  static inline std::string
  demangle(const char* name) {
    return std::string(name);
  }
  #endif
  
  static void symbol_tables( std::ostream& out, const elfio& reader, SYM_TAB& sym_tab ) {
      Elf_Half n = reader.sections.size();
      for ( Elf_Half i = 0; i < n; ++i ) {    // For all sections
          section* sec = reader.sections[i];
          if ( SHT_SYMTAB == sec->get_type() || SHT_DYNSYM == sec->get_type() ) {
              symbol_section_accessor symbols( reader, sec );
  
              Elf_Xword     sym_no = symbols.get_symbols_num();
              if ( sym_no > 0 ) {
                  out << "Symbol table (" << sec->get_name() << ")" << std::endl;
                  /*
                  if ( reader.get_class() == ELFCLASS32 ) { // Output for 32-bit
                      out << "[  Nr ] Value    Size     Type    Bind      Sect Name"
                          << std::endl;
                  }
                  else {                                    // Output for 64-bit
                      out << "[  Nr ] Value            Size             Type    Bind      Sect" << std::endl
                          << "        Name"
                          << std::endl;
                  }
                  */
                  for ( unsigned i = 0; i < sym_no; ++i ) {
                      std::string   name("");
                      Elf64_Addr    value=0;
                      Elf_Xword     size=0;
                      unsigned char bind=0;
                      unsigned char type=0;
                      Elf_Half      section=0;
                      unsigned char other=0;
                      symbols.get_symbol(i, name, value, size, 
                                         bind, type, section, other);
                      prof_symbol& sym = sym_tab[(uint64_t)value];
                      sym.name=name;
                      sym.value=value;
                      sym.size=size;
                      sym.bind=bind;
                      sym.type=type;
                      sym.section=section;
                      sym.other=other;
  
                      //dump::symbol_table( out, i, name, value, 
                      //       size, bind, type, section, reader.get_class() );
                      
                      //out << "type: " << type << "\n";
                  }
  
                  out << std::endl;
              }
          }
      }
  }
  
  static void read_symbol_tables(char* filename, SYM_TAB& sym_tab) {
    elfio reader;
    if ( !reader.load( filename ) ) {
        printf( "File %s is not found or it is not an ELF file\n", filename );
        exit(1);
    }
  
    symbol_tables(std::cout,reader,sym_tab);
  }
};

#endif //ELPARSE_HH

