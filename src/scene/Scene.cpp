#include "Scene.h"
#include "Fetch.h"
#include "Decoder.h"
#include <nlohmann/json.hpp>
#include <assert.h>

using json = nlohmann::json;

static inline void buildRect(Rect& rect, const json& obj)
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

static inline void buildColor(Color& rect, const json& obj)
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

static inline void buildImage(Scene::ImageData& image, const json& obj, Decoder& decoder)
{
    for (auto& [key, value] : obj.items()) {
        if (key == "sourceRect" && value.is_object()) {
            buildRect(image.sourceRect, value);
        } else if (key == "src" && value.is_string()) {
            image.image = decoder.decode(value.get<std::string>());
        }
    }
}

static inline void buildText(Scene::Item& item, const json& obj)
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

static void build(Scene::Item& item, const json& obj, Decoder& decoder, Scene::Item* parent)
{
    for (auto& [key, value] : obj.items()) {
        if (key == "x") {
            if (value.is_number()) {
                item.geometry.x = value.get<int32_t>();
            } else if (value.is_null() && parent) {
                item.geometry.x = parent->geometry.x;
            }
        } else if (key == "y") {
            if (value.is_number()) {
                item.geometry.y = value.get<int32_t>();
            } else if (value.is_null() && parent) {
                item.geometry.y = parent->geometry.y;
            }
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
        }
    }

    try {
        const auto children = obj.at("children");
        if (children.is_array()) {
            item.children.reserve(children.size());
            for (auto& child : children) {
                assert(child.is_object());
                item.children.push_back(std::make_shared<Scene::Item>());
                build(*item.children.back().get(), child, decoder, &item);
            }
        }
    } catch (const nlohmann::json::out_of_range& err) {
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
        scene.root = std::make_shared<Scene::Item>();

        Decoder decoder(Decoder::Format_Auto);
        build(*scene.root.get(), data, decoder, nullptr);
        return scene;
    } catch (const json::parse_error& error) {
        printf("json parse error\n");
    }

    return Scene();
}
