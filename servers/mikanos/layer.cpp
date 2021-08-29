#include <algorithm>

#include "layer.hpp"
#include "console.hpp"
#include "mouse.hpp"
#include "font.hpp"


namespace {
  template <class T, class U>
  void EraseIf(T& c, const U& pred) {
    auto it = std::remove_if(c.begin(), c.end(), pred);
    c.erase(it, c.end());
  }
}

Layer::Layer(unsigned int id) : id_{id} {}

unsigned int Layer::ID() const {
  return id_;
}

Layer& Layer::SetWindow(const std::shared_ptr<Window>& window) {
  window_ = window;
  return *this;
}

std::shared_ptr<Window> Layer::GetWindow() const {
  return window_;
}

Vector2D<int> Layer::GetPosition() const {
  return pos_;
}

Layer& Layer::SetDraggable(bool draggable) {
  draggable_ = draggable;
  return *this;
}

bool Layer::IsDraggable() const {
  return draggable_;
}


Layer& Layer::Move(Vector2D<int> pos) {
  pos_ = pos;
  return *this;
}

Layer& Layer::MoveRelative(Vector2D<int> pos_diff) {
  pos_ += pos_diff;
  return *this;
}

void Layer::DrawTo(PixelWriter& writer, const Rectangle<int>& area) const {
  if (window_) {
    window_->DrawTo(writer, pos_, area);
    
  }
}

void LayerManager::SetWriter(PixelWriter* writer) {
  writer_ = writer;
}

Layer& LayerManager::NewLayer() {
  ++latest_id_;
  return *layers_.emplace_back(new Layer{latest_id_});
}

void LayerManager::RemoveLayer(unsigned int id) {
  Hide(id);

  auto pred = [id](const std::unique_ptr<Layer>& elem) {
    return elem->ID() == id;
  };

  EraseIf(layers_, pred);

  std::vector<unsigned int>::iterator e = remove(window_layer_id.begin(), window_layer_id.end(), id);
  window_layer_id.erase(e, window_layer_id.end());
}

void LayerManager::Draw(const Rectangle<int>& area) const {
  for (auto layer : layer_stack_) {
    layer->DrawTo(*writer_, area);
  }
}

void LayerManager::Draw(unsigned int id) const {
  bool draw = false;
  Rectangle<int> window_area;
  for (auto layer : layer_stack_) {
    if (layer->ID() == id) {
      window_area.size = layer->GetWindow()->Size();
      window_area.pos = layer->GetPosition();
      draw = true;
    }
    if (draw) {
      layer->DrawTo(*writer_, window_area);
    }
  }
}



void LayerManager::Move(unsigned int id, Vector2D<int> new_pos) {
  auto layer = FindLayer(id);
  const auto window_size = layer->GetWindow()->Size();
  const auto old_pos = layer->GetPosition();
  layer->Move(new_pos);
  Draw({old_pos, window_size});
  Draw(id);
}

void LayerManager::MoveRelative(unsigned int id, Vector2D<int> pos_diff) {
  auto layer = FindLayer(id);
  const auto window_size = layer->GetWindow()->Size();
  const auto old_pos = layer->GetPosition();
  layer->MoveRelative(pos_diff);
  Draw({old_pos, window_size});
  Draw(id);
}

void LayerManager::UpDown(unsigned int id, int new_height) {
  if (new_height < 0) {
    Hide(id);
    return;
  }
  if (new_height > layer_stack_.size()) {
    new_height = layer_stack_.size();
  }

  auto layer = FindLayer(id);
  auto old_pos = std::find(layer_stack_.begin(), layer_stack_.end(), layer);
  auto new_pos = layer_stack_.begin() + new_height;

  if (old_pos == layer_stack_.end()) {
    layer_stack_.insert(new_pos, layer);
    return;
  }

  if (new_pos == layer_stack_.end()) {
    --new_pos;
  }
  layer_stack_.erase(old_pos);
  layer_stack_.insert(new_pos, layer);
}

void LayerManager::Hide(unsigned int id) {
  auto layer = FindLayer(id);
  auto pos = std::find(layer_stack_.begin(), layer_stack_.end(), layer);
  if (pos != layer_stack_.end()) {
    layer_stack_.erase(pos);
  }
}

Layer* LayerManager::FindLayerByPosition(Vector2D<int> pos, unsigned int exclude_id) const {
  auto pred = [pos, exclude_id](Layer* layer) {
    if (layer->ID() == exclude_id) {
      return false;
    }
    const auto& win = layer->GetWindow();
    if (!win) {
      return false;
    }
    const auto win_pos = layer->GetPosition();
    const auto win_end_pos = win_pos + win->Size();
    return win_pos.x <= pos.x && pos.x < win_end_pos.x &&
           win_pos.y <= pos.y && pos.y < win_end_pos.y;
  };
  auto it = std::find_if(layer_stack_.rbegin(), layer_stack_.rend(), pred);
  if (it == layer_stack_.rend()) {
    return nullptr;
  }
  return *it;
}

Layer* LayerManager::FindLayer(unsigned int id) {
  auto pred = [id](const std::unique_ptr<Layer>& elem) {
    return elem->ID() == id;
  };
  auto it = std::find_if(layers_.begin(), layers_.end(), pred);
  if (it == layers_.end()) {
    return nullptr;
  }
  return it->get();
}

int LayerManager::GetHeight(unsigned int id) {
  for (int h = 0; h < layer_stack_.size(); ++h) {
    if (layer_stack_[h]->ID() == id) {
      return h;
    }
  }
  return -1;
}

LayerManager* layer_manager;


ActiveLayer::ActiveLayer(LayerManager& manager) : manager_{manager} {
}

void ActiveLayer::SetMouseLayer(unsigned int mouse_layer) {
  mouse_layer_ = mouse_layer;
}

void ActiveLayer::Activate(unsigned int layer_id) {
  if (active_layer_ == layer_id) {
    return;
  }

  if (active_layer_ > 0) {
    Layer* layer = manager_.FindLayer(active_layer_);
    layer->GetWindow()->Deactivate();
    manager_.Draw(active_layer_);
    
  }

  active_layer_ = layer_id;
  if (active_layer_ > 0) {
    Layer* layer = manager_.FindLayer(active_layer_);
    layer->GetWindow()->Activate();
    manager_.UpDown(active_layer_, 0);
    manager_.UpDown(active_layer_, manager_.GetHeight(mouse_layer_) - 1);
    manager_.Draw(active_layer_);
  }
}

ActiveLayer* active_layer;
std::vector<unsigned int> window_layer_id;
std::map<unsigned int, uint64_t>* layer_task_map;

void InitializeLayer() {
    auto bgwindow = std::make_shared<Window>(ScreenSize().x, ScreenSize().y);
    DrawDesktop(*bgwindow->Writer());

    auto console_window = std::make_shared<Window>(
      Console::kColumns * 8, Console::kRows * 16);
    console->SetWindow(console_window);

    layer_manager = new LayerManager;
    layer_manager->SetWriter(screen_writer);

    auto bglayer_id = layer_manager->NewLayer()
        .SetWindow(bgwindow)
        .Move({0, 0})
        .ID();
        
    console->SetLayerID(layer_manager->NewLayer()
    .SetWindow(console_window)
    .Move({0, 0})
    .ID());


    layer_manager->UpDown(bglayer_id, 0);
    layer_manager->UpDown(console->LayerID(), 1);

    active_layer = new ActiveLayer{*layer_manager};
}