
#ifndef MAGIC_HOME_H
#define MAGIC_HOME_H
#include <stdint.h>
#include <string>
#include <vector>
#include "Light.h"
class MagicHome {
    public:
    MagicHome();
    void Init();
    void DiscoverLights();
    std::vector<Light> DiscoveredLights();
    private:
    std::vector<Light> lights;
};

#endif