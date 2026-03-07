from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout


class FwConan(ConanFile):
    name = "fw"
    version = "0.1.0"
    package_type = "header-library"

    license = "MIT"
    url = "https://github.com/andrian/function_wrapper"
    description = "Header-only callable wrapper with multi-signature dispatch."
    topics = ("function-wrapper", "callable", "type-erasure", "header-only")

    settings = "os", "arch", "compiler", "build_type"
    options = {"with_tests": [True, False]}
    default_options = {"with_tests": False}

    exports_sources = (
        "CMakeLists.txt",
        "CMakePresets.json",
        "fwConfig.cmake.in",
        "include/*",
        "tests/*",
    )
    no_copy_source = True

    def layout(self):
        cmake_layout(self)

    def package_id(self):
        self.info.clear()

    def build_requirements(self):
        if self.options.with_tests:
            self.test_requires("gtest/1.15.0")

    def generate(self):
        toolchain = CMakeToolchain(self)
        toolchain.variables["FW_BUILD_TESTS"] = self.options.with_tests
        toolchain.variables["BUILD_TESTING"] = self.options.with_tests
        toolchain.generate()

        dependencies = CMakeDeps(self)
        dependencies.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "fw")
        self.cpp_info.set_property("cmake_target_name", "fw::wrapper")
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
