# Compiler tests

White-box/unit tests for the bootstrap compiler live here. User-visible language contracts must also be represented independently in the sibling `strut-regression-suite` repository.

HTTP application checkpoints additionally require focused black-box dogfood coverage for plaintext and TLS, the complete serial HTTP compatibility chain, GCC and Clang CTest, GCC ASan/UBSan, hosted cross-platform/release workflow wiring, and an independent regression-suite case for the public API contract.
