#ifndef SCENE_H
#define SCENE_H

#include <string>
#include <vector>
#include <memory>
#include "Color.h"
#include "Image.h"
#include "Text.h"
#include "Rect.h"

class Scene
{
public:
    class ImageData
    {
    public:
        Rect sourceRect;
        std::shared_ptr<Image> image;
    };

    class Item
    {
    public:
        Color color;
        Rect geometry;
        Text text;

        ImageData image, backgroundImage;
        std::vector<std::shared_ptr<Item> > children;
    };

    std::shared_ptr<Item> root;

    static Scene sceneFromJSON(const std::string& path);
};

#endif // SCENE_H
