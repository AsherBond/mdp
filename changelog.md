
## Changes on Friday, July 26, 2024 05:24:24 by Asher Bond- Version display command added. Changelog updated. Build workflow established.



Commit hash: fad734b3ff0da6c64c36211583ae96076516c7dc
Date: Friday, July 26, 2024 05:14:48
Author: Asher Bond
Message: "Version display command added; changelog updated."
---
Changelog:
- Added: Run `$PREFIX/mdp -v` to display the version of the installed project.
- Changelog file was modified to include the above change.
- Added a new file: `.github/workflows/build.yml`  
- This file sets up a GitHub Actions workflow named Build CI that triggers on push and pull request events.

 Changelog:
- Added: Run `$PREFIX/mdp -v` to display the version of the installed project.
- Changelog file was modified to include the above change.
Changelog:
- Added a new file: `.github/workflows/build.yml`  
- This file sets up a GitHub Actions workflow namedBuild CI that triggers on push and pull request events.  
- The workflow runs on the latest Ubuntu environment and includes a build job with a matrix strategy for multiple compilers (gcc-10, gcc-11, clang-13, clang-14).  
- The job sets the environment variable `PREFIX` to `/usr/local/bin` and `CC` to the respective compiler from the matrix.  
- The job includes the following steps:    
- Checkout the code using `actions/checkout@v3`.    
- Display the compiler version using `${CC} -v`.    
- Run `make` to build the project.    
- Run `sudo make install` to install the built project.    
- Run `ldd $PREFIX/mdp` to display the shared library dependencies.    
- Run `$PREFIX/mdp -v` to display the version of the installed project.
Changelog:
- Added a new file: `.github/workflows/build.yml`  
- This file sets up a GitHub Actions workflow namedBuild CI that triggers on push and pull request events.  
- The workflow runs on the latest Ubuntu environment and includes a build job with a matrix strategy for multiple compilers (gcc-10, gcc-11, clang-13, clang-14).  
- The job sets the environment variable `PREFIX` to `/usr/local/bin` and `CC` to the respective compiler from the matrix.  
- The job includes the following steps:    
- Checkout the code using `actions/checkout@v3`.    
- Display the compiler version using `${CC} -v`.    
- Run `make` to build the project.    
- Run `sudo make install` to install the built project.    
- Run `ldd $PREFIX/mdp` to display the shared library dependencies.    
- Run `$PREFIX/mdp -v` to display the version of the installed project.
Changelog:
- Added a new file: `.github/workflows/build.yml`
- This file sets up a GitHub Actions workflow namedBuild CI that triggers on push and pull request events.
- The workflow runs on the latest Ubuntu environment and includes a build job with a matrix strategy for multiple compilers (gcc-10, gcc-11, clang-13, clang-14).
- The job sets the environment variable `PREFIX` to `/usr/local/bin` and `CC` to the respective compiler from the matrix.
- The job includes the following steps:  
- Checkout the code using `actions/checkout@v3`.  
- Display the compiler version using `${CC} -v`.  
- Run `make` to build the project.  
- Run `sudo make install` to install the built project.  
- Run `ldd $PREFIX/mdp` to display the shared library dependencies.  
- Run `$PREFIX/mdp -v` to display the version of the installed project.
Changelog:
- Added a new file: `.github/workflows/build.yml`  
- This file sets up a GitHub Actions workflow namedBuild CI that triggers on push and pull request events.  
- The workflow runs on the latest Ubuntu environment and includes a build job with a matrix strategy for multiple compilers (gcc-10, gcc-11, clang-13, clang-14).  
- The job sets the environment variable `PREFIX` to `/usr/local/bin` and `CC` to the respective compiler from the matrix.  
- The job includes the following steps:    
- Checkout the code using `actions/checkout@v3`.    
- Display the compiler version using `${CC} -v`.    
- Run `make` to build the project.    
- Run `sudo make install` to install the built project.    
- Run `ldd $PREFIX/mdp` to display the shared library dependencies.    
- Run `$PREFIX/mdp -v` to display the version of the installed project.
Changelog:
- Added a new file: `.github/workflows/build.yml`  
- This file sets up a GitHub Actions workflow namedBuild CI that triggers on push and pull request events.  
- The workflow runs on the latest Ubuntu environment and includes a build job with a matrix strategy for multiple compilers (gcc-10, gcc-11, clang-13, clang-14).  
- The job sets the environment variable `PREFIX` to `/usr/local/bin` and `CC` to the respective compiler from the matrix.  
- The job includes the following steps:    
- Checkout the code using `actions/checkout@v3`.    
- Display the compiler version using `${CC} -v`.    
- Run `make` to build the project.    
- Run `sudo make install` to install the built project.    
- Run `ldd $PREFIX/mdp` to display the shared library dependencies.    
- Run `$PREFIX/mdp -v` to display the version of the installed project.
Changelog:
- Added a new file: `.github/workflows/build.yml`  
- This file sets up a GitHub Actions workflow named "Build CI" that triggers on push and pull request events.  
- The workflow runs on the latest Ubuntu environment and includes a build job with a matrix strategy for multiple compilers (gcc-10, gcc-11, clang-13, clang-14).  
- The job sets the environment variable `PREFIX` to `/usr/local/bin` and `CC` to the respective compiler from the matrix.  
- The job includes the following steps:    
- Checkout the code using `actions/checkout@v3`.    
- Display the compiler version using `${CC} -v`.    
- Run `make` to build the project.    
- Run `sudo make install` to install the built project.    
- Run `ldd $PREFIX/mdp` to display the shared library dependencies.    
- Run `$PREFIX/mdp -v` to display the version of the installed project.

Changelog:
- Added: Run `$PREFIX/mdp -v` to display the version of the installed project.
-e Changelog:
- Added: Run `$PREFIX/mdp -v` to display the version of the installed project.
-e Changelog:
- Added: Run `$PREFIX/mdp -v` to display the version of the installed project.
Changelog:
- Added: Run `$PREFIX/mdp -v` to display the version of the installed project.
- Changelog file was modified to include the above change.
