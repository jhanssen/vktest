#include "Scene.h"
#include "Fetch.h"
#include "Decoder.h"
#include <nlohmann/json.hpp>
#include <assert.h>

using json = nlohmann::json;

static void buildRect(Rect& rect, const json& obj)
{
    for (auto& [key, value] : obj.items()) {
        if (key == "x" && value.is_number()) {
            rect.x = value.get<int32_t>();
        } else if (key == "y" && value.is_number()) {
            rect.y = value.get<int32_t>();
        } else if (key == "width" && value.is_number()) {
            rect.width = value.get<uint32_t>();
        } else if (key == "height" && value.is_number()) {
            rect.height = value.get<uint32_t>();
        }
    }
}

static void buildColor(Color& rect, const json& obj)
{
    for (auto& [key, value] : obj.items()) {
        if (key == "r" && value.is_number()) {
            rect.r = value.get<uint32_t>() / 255.f;
        } else if (key == "g" && value.is_number()) {
            rect.g = value.get<uint32_t>() / 255.f;
        } else if (key == "b" && value.is_number()) {
            rect.b = value.get<uint32_t>() / 255.f;
        } else if (key == "a" && value.is_number()) {
            rect.a = value.get<uint32_t>() / 255.f;
        }
    }
}

static void buildImage(Scene::ImageData& image, const json& obj, Decoder& decoder)
{
    for (auto& [key, value] : obj.items()) {
        if (key == "sourceRect" && value.is_object()) {
            buildRect(image.sourceRect, value);
        } else if (key == "src" && value.is_string()) {
            image.image = decoder.decode(value.get<std::string>());
        }
    }
}

static void buildText(Scene::Item& item, const json& obj)
{
    for (auto& [key, value] : obj.items()) {
        if (key == "color" && value.is_object()) {
            buildColor(item.text.color, value);
        } else if (key == "contents" && value.is_string()) {
            item.text.contents = value.get<std::string>();
        } else if (key == "size" && value.is_number()) {
            item.text.size = value.get<uint32_t>();
        } else if (key == "weight" && value.is_string()) {
            item.text.bold = value.get<std::string>() == "bold";
        } else if (key == "style" && value.is_string()) {
            item.text.italic = value.get<std::string>() == "italic";
        }
    }
}

static void build(Scene::Item& item, const json& obj, Decoder& decoder)
{
    for (auto& [key, value] : obj.items()) {
        if (key == "x" && value.is_number()) {
            item.geometry.x = value.get<int32_t>();
        } else if (key == "y" && value.is_number()) {
            item.geometry.y = value.get<int32_t>();
        } else if (key == "width" && value.is_number()) {
            item.geometry.width = value.get<uint32_t>();
        } else if (key == "height" && value.is_number()) {
            item.geometry.height = value.get<uint32_t>();
        } else if (key == "backgroundColor" && value.is_object()) {
            buildColor(item.color, value);
        } else if (key == "images" && value.is_array()) {
            for (const auto& img : value) {
                const auto isbg = img.value("background", false);
                if (isbg) {
                    buildImage(item.backgroundImage, img, decoder);
                } else {
                    buildImage(item.image, img, decoder);
                }
            }
        } else if (key == "text" && value.is_object()) {
            buildText(item, value);
        } else if (key == "children" && value.is_array()) {
            item.children.resize(value.size());
            uint32_t idx = 0;
            for (auto& child : value) {
                assert(child.is_object());
                build(item.children[idx++], child, decoder);
            }
        }
    }
}

Scene Scene::sceneFromJSON(const std::string& path)
{
    const Buffer jsondata = Fetch::fetch(path);

    try {
        auto data = json::parse(jsondata.data(), jsondata.data() + jsondata.size());
        if (!data.is_object()) {
            return Scene();
        }

        Scene scene;
        Decoder decoder(Decoder::Format_Auto);
        build(scene.root, data, decoder);
        return scene;
    } catch (const json::parse_error& error) {
        printf("json parse error\n");
    }

    return Scene();
}
