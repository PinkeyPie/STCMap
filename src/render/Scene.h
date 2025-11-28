#pragma once

#include "../pch.h"
#include <entt/entt.hpp>
#include <entt/entity/registry.hpp>

struct TagComponent {
    char Name[16];

    TagComponent(const char* name) {
        strncpy(Name, name, sizeof(Name));
    }
};

struct SceneEntity {
    SceneEntity() = default;
    inline SceneEntity(entt::entity handle, struct Scene& scene);
    inline SceneEntity(uint32 id, struct Scene& scene);
    SceneEntity(const SceneEntity&) = default;

    template<class ComponentT>
    bool HasComponent() {
        return _registry->has<ComponentT>(_handle);
    }

    template<class ComponentT, class... Args>
    SceneEntity& AddComponent(Args&&... a) {
        _registry->emplace<ComponentT>(_handle, std::forward<Args>(a)...);
        return *this;
    }

    template<class ComponentT>
    ComponentT& GetComponent() {
        return _registry->get<ComponentT>(_handle);
    }

    template<class ComponentT>
    void RemoveComponent() {
        _registry->remove<ComponentT>(_handle);
    }

    operator uint32() {
        return (uint32)_handle;
    }

    operator bool() const {
        return _handle != entt::null;
    }

    bool operator==(const SceneEntity& o) const {
        return _handle == o._handle and _registry == o._registry;
    }

    bool operator==(entt::entity o) const {
        return _handle == o;
    }

private:
    entt::entity _handle = entt::null;
    entt::registry* _registry;

    SceneEntity(entt::entity handle, entt::registry* registry) : _handle(handle), _registry(registry) {}

    friend struct Scene;
};

struct Scene {
    SceneEntity CreateEntity(const char* name) {
        return SceneEntity(_registry.create(), &_registry).AddComponent<TagComponent>(name);
    }

    void DeleteEntity(SceneEntity e) {
        _registry.destroy(e._handle);
    }

    template<class... ComponentT>
    auto view() {
        return _registry.view<ComponentT...>();
    }

    template<class... OwnedComponentT, class... Get, class... Exclude>
    auto group(entt::get_t<Get...>, entt::exclude_t<Exclude...> = {}) {
        return _registry.group<OwnedComponentT...>(entt::get<Get...>, entt::exclude<Exclude...>);
    }

private:
    entt::registry _registry;

    friend SceneEntity;
};

inline SceneEntity::SceneEntity(entt::entity handle, struct Scene &scene) : _handle(handle), _registry(&scene._registry) {}
inline SceneEntity::SceneEntity(uint32 id, struct Scene &scene) : _handle(entt::entity(id)), _registry(&scene._registry) {}

