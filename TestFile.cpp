///////////////////////////////////////////////////////////////////////////////
//
//  TestFile.cpp
//
//  Copyright © Pete Isensee (PKIsensee@msn.com).
//  All rights reserved worldwide.
//
//  Permission to copy, modify, reproduce or redistribute this source code is
//  granted provided the above copyright notice is retained in the resulting 
//  source code.
// 
//  This software is provided "as is" and without any express or implied
//  warranties.
//
///////////////////////////////////////////////////////////////////////////////

#include <chrono>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iostream>
#include <string>

#include "File.h"
#include "Log.h"
#include "StrUtil.h"
#include "Util.h"

using namespace PKIsensee;
namespace fs = std::filesystem;

#ifdef _DEBUG
#define test(e) assert(e)
#else
#define test(e) static_cast<void>( (e) || ( Util::DebugBreak(), 0 ) )
#endif

void TestFile()
{
  File f;
  test( !f.IsOpen() );
  test( f.GetLength() == 0 );
  
  f.SetFile( fs::path( "TestFile.cpp" ) ); // open self
  File::Times ft;
  test( f.GetFileTimes( ft ) );
  test( ft.lastWriteTime );
  test( ft.creationTime );
  test( ft.lastAccessTime );
  test( f.Open( FileFlags::Read | FileFlags::SharedRead ) );
  test( f.IsOpen() );
  test( f.GetLength() > 0 );
  File::Times ftb;
  test( f.GetFileTimes( ftb ) );
  test( ft.lastWriteTime == ftb.lastWriteTime );
  test( ft.creationTime == ftb.creationTime );
  test( ft.lastAccessTime != ftb.lastAccessTime );

  char buffer[ 1024 ] = {};
  test( f.Read( buffer, 1024 ) );
  for( int i = 0; i < 70; ++i )
    test( buffer[i] == '/' );
  test( f.SetPos( 1234u ) );
  test( f.Read( buffer, 1024 ) );
  test( f.SetPos( 0u ) );
  test( f.Read( buffer, 1024 ) );
  test( buffer[ 0 ] == '/' );
    
  test( !f.Write( buffer, 1 ) ); // didn't open for writing
  f.Close();
  test( !f.IsOpen() );
  
  File g( fs::path( "temp.tmp" ) );
  g.Delete();
  test( g.Create( FileFlags::Write ) );
  for( int i = 0; i < 1024; ++i )
    buffer[i] = (char)i;
  test( g.Write( buffer, 1024 ) );
  test( g.GetLength() == 1024 );
  g.Flush();
  g.Close();
  test( g.GetLength() == 1024 );
  test( g.Open( FileFlags::Read ) );
  test( g.Read( buffer, 1024 ) );
  for( int i = 0; i < 1024; ++i )
    test( buffer[i] == (char)i );
  g.Close();
  fs::rename( g.GetPath(), fs::path( "temp.tmp.rename" ) );
  g.SetFile( fs::path( "temp.tmp.rename" ) );
  test( g.Delete() );

  g.SetFile( fs::path( "create.this.unusual\\very.long.path\\and.file" ) );
  test( g.Create( FileFlags::Write ) );
  g.Close();
  test( g.Delete() );
  
  // Test directory functions
  g.SetFile( fs::path( "temp.for.testing.rename\\" ) );
  g.Delete();
  g.SetFile( fs::path( "temp.for.testing\\" ) );
  g.Delete();
  test( g.Create( FileFlags::Read | FileFlags::Write ) );
  test( g.IsOpen() );
  g.Close();
  test( g.Open( FileFlags::Read ) );
  test( g.IsOpen() );
  test( g.GetLength() == 0 );
  g.Close();
  test( g.Delete() );

  g.SetFile( fs::path( "create.this.unusual\\very.long.path\\folder\\" ) );
  test( g.Create( FileFlags::Write ) );
  g.Close();
  test( g.Delete() );
  g.SetFile( fs::path( "create.this.unusual" ) );
  test( g.Delete() );

  // Test copy
  fs::path src( "TestFile.cpp" );
  fs::path dst( "TestCopy.cpp" );

  test( fs::copy_file( src, dst, fs::copy_options::overwrite_existing ) );

  // Validate copy
  f.SetFile( src );
  g.SetFile( dst );
  test( f.Open( FileFlags::Read | FileFlags::SharedRead | FileFlags::SequentialScan ) );
  test( g.Open( FileFlags::Read | FileFlags::RandomAccess ) );
  char destBuffer[ 1024 ] = {};
  uint32_t bytesReadSrc = 0;
  do
  {
    bytesReadSrc = 0;
    uint32_t bytesReadDst = 0;
    f.Read( buffer, 1024, bytesReadSrc );
    g.Read( destBuffer, 1024, bytesReadDst );
    test( bytesReadSrc == bytesReadDst );
    test( memcmp( buffer, destBuffer, bytesReadSrc ) == 0 );
  } while( bytesReadSrc > 0 );
  f.Close();

  // Read entire file
  std::string entireFile;
  File::ReadEntireFile( src, entireFile );
  test( entireFile.size() > 0 );
  test( entireFile[ 0 ] == '/' );
  test( entireFile.find( "TestFile.cpp" ) != std::string::npos );

  g.Close();
  g.Delete();
}

void TestTree()
{
  // Use recursive_directory_iterator to do the same thing
  std::filesystem::path currentDir = std::filesystem::current_path();
  for( const auto& entry : std::filesystem::recursive_directory_iterator( currentDir ) )
  {
    std::filesystem::path path = entry.path();
    printf( "%S\n", path.c_str() );
    test( !path.filename().empty() );
  }
}

void measure( const std::string& test, std::function<void()> fn )
{
  auto start = std::chrono::high_resolution_clock::now();
  fn();
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>( end - start );
  std::cout << test << ": " << static_cast<double>( duration.count() ) * 0.000001 << " ms\n";
}

constexpr uint32_t kBufferSize = 1024 * 1024 * 512; // 512 MB

constexpr size_t kInternalBufferSize = 1024 * 1024 * 1;
char internalBuffer[ kInternalBufferSize ];

void TestPerf()
{
  std::vector<uint8_t> buffer( kBufferSize, 0xEE );

  measure( "File write", [buffer]()
  {
    File f( fs::path( "TestPerf.bin" ) );
    test( f.Create( FileFlags::Write ) );
    test( f.Write( buffer.data(), kBufferSize ) );
    f.Close();
  } );

  measure( "fstream write", [buffer]()
  {
    std::fstream f( "TestPerf.bin", std::fstream::out | std::fstream::binary | std::fstream::trunc );
    f.write( (const char*)( buffer.data() ), kBufferSize );
    f.close();
  } );

  measure( "fopen write", [buffer]()
  {
    FILE* f = (FILE*)(1); // avoid weird compiler error
    test( fopen_s( &f, "TestPerf.bin", "wb" ) == 0 );
    if( f != NULL )
    {
      test( fwrite( buffer.data(), kBufferSize, 1, f ) );
      fclose( f );
    }
  } );

  measure( "File read", [buffer]()
  {
    File f( fs::path( "TestPerf.bin" ) );
    test( f.Open( FileFlags::Read | FileFlags::SequentialScan ) );
    test( f.Read( (void*)buffer.data(), kBufferSize ) );
    f.Close();
  } );

  measure( "fopen read", [buffer]()
  {
    FILE* f = (FILE*)( 1 ); // avoid weird compiler error
    test( fopen_s( &f, "TestPerf.bin", "rb" ) == 0 );
    if( f != NULL )
    {
      test( fread( (void*)buffer.data(), kBufferSize, 1, f ) );
      fclose( f );
    }
  } );

  measure( "fstream read", [buffer]()
  {
    std::fstream f( "TestPerf.bin", std::fstream::in | std::fstream::binary );
    f.rdbuf()->pubsetbuf( internalBuffer, kInternalBufferSize );
    test( f.read( (char*)( buffer.data() ), kBufferSize ) );
    f.close();
  } );

  test( std::filesystem::remove( "TestPerf.bin" ) );
}

int __cdecl main()
{
  TestFile();
  TestTree();
  TestPerf();
}
