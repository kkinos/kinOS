#pragma once

#include <map>
#include <memory>
#include <vector>

#include "graphics.hpp"
#include "window.hpp"

class Layer {
   public:
    Layer(unsigned int id = 0);

    unsigned int ID() const;

    Layer& SetWindow(const std::shared_ptr<Window>& window);

    std::shared_ptr<Window> GetWindow() const;

    Vector2D<int> GetPosition() const;

    Layer& SetDraggable(bool draggable);

    bool IsDraggable() const;

    Layer& Move(Vector2D<int> pos);

    Layer& MoveRelative(Vector2D<int> pos_diff);

    void DrawTo(PixelWriter& writer, const Rectangle<int>& area) const;

   private:
    unsigned int id_;
    Vector2D<int> pos_;
    std::shared_ptr<Window> window_;
    bool draggable_{false};
};

class LayerManager {
   public:
    void SetWriter(PixelWriter* writer);

    Layer& NewLayer();

    void RemoveLayer(unsigned int id);

    void Draw(const Rectangle<int>& area) const;

    void Draw(unsigned int id) const;

    void Move(unsigned int id, Vector2D<int> new_pos);

    void MoveRelative(unsigned int id, Vector2D<int> pos_diff);

    void UpDown(unsigned int id, int new_height);

    void Hide(unsigned int id);

    Layer* FindLayerByPosition(Vector2D<int> pos,
                               unsigned int exclude_id) const;

    Layer* FindLayer(unsigned int id);

    int GetHeight(unsigned int id);

   private:
    PixelWriter* writer_{nullptr};
    std::vector<std::unique_ptr<Layer>> layers_{};
    std::vector<Layer*> layer_stack_{};
    unsigned int latest_id_{0};
};

extern LayerManager* layer_manager;

class ActiveLayer {
   public:
    ActiveLayer(LayerManager& manager);
    void SetMouseLayer(unsigned int mouse_layer);
    void Activate(unsigned int layer_id);
    unsigned int GetActive() const { return active_layer_; };

   private:
    LayerManager& manager_;
    unsigned int active_layer_{0};
    unsigned int mouse_layer_{0};
};

extern ActiveLayer* active_layer;
extern std::vector<unsigned int> window_layer_id;
extern std::map<unsigned int, uint64_t>* layer_task_map;

void InitializeLayer();