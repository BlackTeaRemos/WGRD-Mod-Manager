set(WGRD_REGISTRY_DIR "${CMAKE_SOURCE_DIR}/registry" CACHE PATH "Key registry checkout")

set(WGRD_REGISTRY_PUBLIC_KEY_HEX_LENGTH 64)
set(WGRD_REGISTRY_FINGERPRINT_HEX_LENGTH 16)

function(__WgrdHexToByteInitialiser HEX OUTPUT)
    string(LENGTH "${HEX}" hexLength)
    math(EXPR byteCount "${hexLength} / 2")

    set(parts "")
    set(index 0)
    while(index LESS byteCount)
        math(EXPR offset "${index} * 2")
        string(SUBSTRING "${HEX}" ${offset} 2 pair)
        list(APPEND parts "0x${pair}")
        math(EXPR index "${index} + 1")
    endwhile()

    string(REPLACE ";" ", " joined "${parts}")
    set(${OUTPUT} "${joined}" PARENT_SCOPE)
endfunction()

function(__WgrdReadRevokedFingerprints OUTPUT)
    set(fingerprints "")

    file(GLOB revocationFiles "${WGRD_REGISTRY_DIR}/revoked/*.json")
    foreach(path IN LISTS revocationFiles)
        file(READ "${path}" content)
        string(JSON fingerprint GET "${content}" fingerprint)
        list(APPEND fingerprints "${fingerprint}")
    endforeach()

    set(${OUTPUT} "${fingerprints}" PARENT_SCOPE)
endfunction()

function(wgrd_generate_registry_baseline)
    if(NOT IS_DIRECTORY "${WGRD_REGISTRY_DIR}/keys")
        message(FATAL_ERROR
            "registry checkout missing\n"
            "  ${WGRD_REGISTRY_DIR}\n"
            "run git submodule update --init --recursive")
    endif()

    __WgrdReadRevokedFingerprints(revokedFingerprints)

    file(GLOB keyFiles "${WGRD_REGISTRY_DIR}/keys/*.json")
    list(SORT keyFiles)

    set(entries "")
    set(keyCount 0)
    set(revokedCount 0)

    foreach(path IN LISTS keyFiles)
        file(READ "${path}" content)

        string(JSON fingerprint GET "${content}" fingerprint)
        string(JSON publicKey GET "${content}" publicKey)
        string(JSON publisher GET "${content}" publisher)

        string(LENGTH "${fingerprint}" fingerprintLength)
        if(NOT fingerprintLength EQUAL WGRD_REGISTRY_FINGERPRINT_HEX_LENGTH)
            message(FATAL_ERROR "bad fingerprint length\n  ${path}")
        endif()

        string(LENGTH "${publicKey}" publicKeyLength)
        if(NOT publicKeyLength EQUAL WGRD_REGISTRY_PUBLIC_KEY_HEX_LENGTH)
            message(FATAL_ERROR "bad key length\n  ${path}")
        endif()

        __WgrdHexToByteInitialiser("${publicKey}" byteInitialiser)

        if("${fingerprint}" IN_LIST revokedFingerprints)
            set(revoked "true")
            math(EXPR revokedCount "${revokedCount} + 1")
        else()
            set(revoked "false")
        endif()

        string(APPEND entries
            "    BaselineKey{\"${fingerprint}\", \"${publisher}\", ${revoked},\n"
            "        {${byteInitialiser}}},\n")

        math(EXPR keyCount "${keyCount} + 1")
    endforeach()

    set(WGRD_REGISTRY_KEY_COUNT "${keyCount}")
    set(WGRD_REGISTRY_ENTRIES "${entries}")

    configure_file(
        "${CMAKE_CURRENT_LIST_DIR}/RegistryBaseline.h.in"
        "${CMAKE_BINARY_DIR}/generated/domain/RegistryBaseline.h"
        @ONLY
    )

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${keyFiles})

    message(STATUS "wgrd registry ${keyCount} keys ${revokedCount} revoked")
endfunction()

wgrd_generate_registry_baseline()
