set(SOURCES AddonsDirectory.cpp
            CacheStrategy.cpp
            CDDADirectory.cpp
            CDDAFile.cpp
            CircularCache.cpp
            CurlFile.cpp
            DAAPDirectory.cpp
            DAAPFile.cpp
            DAVCommon.cpp
            DAVDirectory.cpp
            DAVFile.cpp
            DirectoryCache.cpp
            Directory.cpp
            DirectoryFactory.cpp
            DirectoryHistory.cpp
            DllLibCurl.cpp
            FavouritesDirectory.cpp
            FileCache.cpp
            File.cpp
            FileDirectoryFactory.cpp
            FileFactory.cpp
            FileReaderFile.cpp
            FTPDirectory.cpp
            FTPParse.cpp
            HTSPDirectory.cpp
            HTSPSession.cpp
            HTTPDirectory.cpp
            HTTPFile.cpp
            IDirectory.cpp
            IFile.cpp
            ImageFile.cpp
            iso9660.cpp
            ISO9660Directory.cpp
            ISOFile.cpp
            LibraryDirectory.cpp
            M4BFileDirectory.cpp
            MemBufferCache.cpp
            MultiPathDirectory.cpp
            MultiPathFile.cpp
            MusicDatabaseDirectory.cpp
            MusicDatabaseFile.cpp
            MusicFileDirectory.cpp
            MusicSearchDirectory.cpp
            MythDirectory.cpp
            MythFile.cpp
            MythSession.cpp
            NptXbmcFile.cpp
            PipeFile.cpp
            PipesManager.cpp
            PlaylistDirectory.cpp
            PlaylistFileDirectory.cpp
            PluginDirectory.cpp
            PVRDirectory.cpp
            PVRFile.cpp
            RSSDirectory.cpp
            SAPDirectory.cpp
            SAPFile.cpp
            ShoutcastFile.cpp
            SmartPlaylistDirectory.cpp
            SMBDirectory.cpp
            SmbFile.cpp
            SourcesDirectory.cpp
            SpecialProtocol.cpp
            SpecialProtocolDirectory.cpp
            SpecialProtocolFile.cpp
            StackDirectory.cpp
            TuxBoxDirectory.cpp
            TuxBoxFile.cpp
            udf25.cpp
            UDFDirectory.cpp
            UDFFile.cpp
            UPnPDirectory.cpp
            UPnPFile.cpp
            VideoDatabaseDirectory.cpp
            VirtualDirectory.cpp
            ZeroconfDirectory.cpp
            ZipDirectory.cpp
            ZipFile.cpp
            ZipManager.cpp)

if(OS STREQUAL "Android")
  list(APPEND SOURCES AndroidAppDirectory.cpp
                      AndroidAppFile.cpp
                      APKDirectory.cpp
                      APKFile.cpp)
endif()

if(BLURAY_FOUND)
  list(APPEND SOURCES BlurayDirectory.cpp)
endif()

include_directories(${CORE_SOURCE_DIR}/lib/libUPnP
                    ${CORE_SOURCE_DIR}/lib/libUPnP/Platinum/Source/Core
                    ${CORE_SOURCE_DIR}/lib/libUPnP/Platinum/Source/Platinum
                    ${CORE_SOURCE_DIR}/lib/libUPnP/Platinum/Source/Devices/MediaConnect
                    ${CORE_SOURCE_DIR}/lib/libUPnP/Platinum/Source/Devices/MediaRenderer
                    ${CORE_SOURCE_DIR}/lib/libUPnP/Platinum/Source/Devices/MediaServer
                    ${CORE_SOURCE_DIR}/lib/libUPnP/Platinum/Source/Extras
                    ${CORE_SOURCE_DIR}/lib/libUPnP/Neptune/Source/System/Posix
                    ${CORE_SOURCE_DIR}/lib/libUPnP/Neptune/Source/Core)

add_definitions(-DNPT_CONFIG_ENABLE_LOGGING)
  
core_add_library(filesystem)
add_dependencies(filesystem libcpluff ffmpeg)
