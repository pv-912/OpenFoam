/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011-2017 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "dlLibraryTable.H"
#include "OSspecific.H"
#include "int.H"

#include <fstream>
#include <sstream>

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
    defineTypeNameAndDebug(dlLibraryTable, 0);
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::dlLibraryTable::dlLibraryTable()
{}


Foam::dlLibraryTable::dlLibraryTable
(
    const dictionary& dict,
    const word& libsEntry
)
{
    open(dict, libsEntry);
}


// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

Foam::dlLibraryTable::~dlLibraryTable()
{
    forAllReverse(libPtrs_, i)
    {
        if (libPtrs_[i])
        {
            if (debug)
            {
                InfoInFunction
                    << "Closing " << libNames_[i]
                    << " with handle " << uintptr_t(libPtrs_[i]) << endl;
            }
            if (!dlClose(libPtrs_[i]))
            {
                WarningInFunction<< "Failed closing " << libNames_[i]
                    << " with handle " << uintptr_t(libPtrs_[i]) << endl;
            }
        }
    }
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

bool Foam::dlLibraryTable::open
(
    const fileName& functionLibName,
    const bool verbose
)
{
    if (functionLibName.size())
    {
        void* functionLibPtr = dlOpen
        (
            fileName(functionLibName).expand(),
            verbose
        );

        if (debug)
        {
            InfoInFunction
                << "Opened " << functionLibName
                << " resulting in handle " << uintptr_t(functionLibPtr) << endl;
        }

        if (!functionLibPtr)
        {
            if (verbose)
            {
                WarningInFunction
                    << "could not load " << functionLibName
                    << endl;
            }

            return false;
        }
        else
        {
            libPtrs_.append(functionLibPtr);
            libNames_.append(functionLibName);
            return true;
        }
    }
    else
    {
        return false;
    }
}


bool Foam::dlLibraryTable::close
(
    const fileName& functionLibName,
    const bool verbose
)
{
    label index = -1;
    forAllReverse(libNames_, i)
    {
        if (libNames_[i] == functionLibName)
        {
            index = i;
            break;
        }
    }

    if (index != -1)
    {
        if (debug)
        {
            InfoInFunction
                << "Closing " << functionLibName
                << " with handle " << uintptr_t(libPtrs_[index]) << endl;
        }

        bool ok = dlClose(libPtrs_[index]);

        libPtrs_[index] = nullptr;
        libNames_[index] = fileName::null;

        if (!ok)
        {
            if (verbose)
            {
                WarningInFunction
                    << "could not close " << functionLibName
                    << endl;
            }

            return false;
        }

        return true;
    }
    return false;
}


void* Foam::dlLibraryTable::findLibrary(const fileName& functionLibName)
{
    label index = -1;
    forAllReverse(libNames_, i)
    {
        if (libNames_[i] == functionLibName)
        {
            index = i;
            break;
        }
    }

    if (index != -1)
    {
        return libPtrs_[index];
    }
    return nullptr;
}


Foam::fileName findLibraryInLibraryPath(const Foam::fileName& library)
{
    // Get library path.
    Foam::string libpath = Foam::getEnv("LD_LIBRARY_PATH"), directory;
    
    // Split by colon.
    Foam::fileNameList directories;
    std::istringstream iss (libpath);
    while (std::getline(iss, directory, ':'))
        directories.append(directory);
    
    // Search the directories for the library.
    forAll(directories, i)
    {
        Foam::fileName abspath = directories[i]/library;
        if (std::ifstream(abspath.c_str()).good())
            return abspath;
    }
    
    // Not found: return empty filename.
    return Foam::fileName();
}


bool Foam::dlLibraryTable::open
(
    const dictionary& dict,
    const word& libsEntry
)
{
    if (dict.found(libsEntry))
    {
        fileNameList libNames(dict.lookup(libsEntry));
#ifndef MSWIN
        // -----------------------------------------------------------------------------
        // CFD Support extension : Load also all product libraries
        // -----------------------------------------------------------------------------
        // It is expected that the products are located in OPENFOAM_IN_BOX_INSTALL_PATH
        // as separate directories.
        //
        regExp rex("(OpenFOAM-.*|ThirdParty-.*)");
        Info << "Reading CFD support product libraries" << endl;
        fileName foamDir = getEnv("OPENFOAM_IN_BOX_INSTALL_PATH");
        string wmOptions = getEnv("WM_OPTIONS");
        if (isDir(foamDir) && wmOptions.size() != 0)
        {
            Info << "  Installation directory: " << foamDir << endl;
            Info << "  Platform: " << wmOptions << endl;
            fileNameList productDirs = readDir(foamDir, fileName::DIRECTORY, false);
            forAll(productDirs, i)
            {
                if (!rex.match(productDirs[i]))
                {
                    Info << "  Product: " << productDirs[i] << endl;
                    fileName productLibDir = foamDir/productDirs[i]/"platforms"/wmOptions/"lib";
                    Info << "    library path: " << productLibDir << endl;
                    fileNameList productLibs = readDir(productLibDir, fileName::FILE, false);
                    forAll(productLibs, j)
                    {
                        Info << "    adding " << findLibraryInLibraryPath(productLibs[j]) << endl;

                        // Alternative 1: Add library by name.
                        // - Allows search for this library in LD_LIBRARY_PATH (i.e. FOAM_USER_LIBBIN etc.).
                        libNames.append(productLibs[j]);

                        // Alternative 2: Add library by path.
                        // - Forces usage of the product's version of this library.
                        //libNames.append(productLibDir/productLibs[j]);
                    }
                }
            }
        }
        Info << endl;
        // -------------------------------------------------------------------------------
#endif
        bool allOpened = !libNames.empty();

        forAll(libNames, i)
        {
            allOpened = dlLibraryTable::open(libNames[i]) && allOpened;
        }

        return allOpened;
    }
    else
    {
        return false;
    }
}


// ************************************************************************* //
