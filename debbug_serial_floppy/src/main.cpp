#include <memory>
#include "Device_t.hpp"

int main(int argc, char* argv[]) {
    auto device = std::make_unique<Device::Device_t>(VERSION);
    return device->run(argc, argv);
}