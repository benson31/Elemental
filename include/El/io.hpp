/*
   Copyright (c) 2009-2016, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License,
   which can be found in the LICENSE file in the root directory, or at
   http://opensource.org/licenses/BSD-2-Clause
*/
#ifndef EL_IO_HPP
#define EL_IO_HPP

#include <fstream>
#include <iostream>

#include "El/core/Matrix/decl.hpp"
#include "El/Types/Enums.hpp"

namespace El
{

const char* QtImageFormat( FileFormat format );
std::string FileExtension( FileFormat format );
FileFormat FormatFromExtension( const std::string ext );
FileFormat DetectFormat( const std::string filename );

std::ifstream::pos_type FileSize( std::ifstream& file );

// TODO: Many more color maps
namespace ColorMapNS {
enum ColorMap
{
    GRAYSCALE,
    GRAYSCALE_DISCRETE,
    RED_BLACK_GREEN,
    BLUE_RED
};
}
using namespace ColorMapNS;

// Color maps
// ==========
void SetColorMap( ColorMap colorMap );
ColorMap GetColorMap();
void SetNumDiscreteColors( Int numColors );
Int NumDiscreteColors();

// Display
// =======
void ProcessEvents( int numMsecs );

// Dense
// -----
template<typename Real>
void Display( const Matrix<Real>& A, std::string title="Matrix" );
template<typename Real>
void Display( const Matrix<Complex<Real>>& A, std::string title="Matrix" );
template<typename T>
void Display( const AbstractDistMatrix<T>& A, std::string title="DistMatrix" );

// Print
// =====

// Dense
// -----
template<typename T>
void Print(
    const Matrix<T>& A, std::string title="Matrix", std::ostream& os=std::cout);
template<typename T>
void Print(
    const AbstractDistMatrix<T>& A, std::string title="DistMatrix",
    std::ostream& os=std::cout );

// Utilities
// ---------
template<typename T>
void Print(
    const std::vector<T>& x, std::string title="vector",
    std::ostream& os=std::cout);

// Read
// ====
template<typename T>
void Read( Matrix<T>& A, const std::string filename, FileFormat format=FileFormat::AUTO );
template<typename T>
void Read
( AbstractDistMatrix<T>& A,
  const std::string filename, FileFormat format=FileFormat::AUTO, bool sequential=false );

// Spy
// ===
template<typename T>
void Spy( const Matrix<T>& A, std::string title="Matrix", Base<T> tol=0 );
template<typename T>
void Spy
( const AbstractDistMatrix<T>& A, std::string title="DistMatrix", Base<T> tol=0 );

// Write
// =====
template<typename T>
void Write
( const Matrix<T>& A, std::string basename="Matrix",
  FileFormat format=FileFormat::BINARY,
  std::string title="" );
template<typename T>
void Write
( const AbstractDistMatrix<T>& A, std::string basename="DistMatrix",
  FileFormat format=FileFormat::BINARY, std::string title="" );

} // namespace El

#ifdef EL_HAVE_QT5

#include "El/io/DisplayWidget.hpp"
#include "El/io/DisplayWindow-premoc.hpp"
#include "El/io/ComplexDisplayWindow-premoc.hpp"

namespace El {

QRgb SampleColorMap( double value, double minVal, double maxVal );

}

#endif // ifdef EL_HAVE_QT5

#endif // ifndef EL_IO_HPP
