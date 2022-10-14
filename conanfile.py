from conan import ConanFile


class Recipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps", "VirtualRunEnv"
    default_options = {
        "fmt/*:header_only": True,
        "boost-ext-ut/*:disable_module": True,
    }

    def layout(self):
        self.folders.build = "build"
        self.folders.generators = "conan"

    def requirements(self):
        self.requires("fmt/9.1.0")
        self.requires("nanorange/20200505")
        self.requires("frozen/1.1.1")
        self.requires("fast_float/3.5.1")

    def build_requirements(self):
        self.test_requires("boost-ext-ut/1.1.9")
        self.test_requires("eigen/3.4.0")
