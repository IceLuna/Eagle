#include "egpch.h"

#include "PhysicsCharacterController.h"
#include "PhysicsScene.h"
#include "PhysicsUtils.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/Components/Components.h"

namespace Eagle
{
    class ControllerFilterCallback : public physx::PxControllerFilterCallback
    {
    public:
        // Filtering method for CCT-vs-CCT.
        // @a	First CCT
        // @b	Second CCT
        // @return true to keep the pair, false to filter it out
        bool filter(const physx::PxController& a, const physx::PxController& b) override
        {
            const PhysicsCharacterController* controllerA = (const PhysicsCharacterController*)a.getUserData();
            const PhysicsCharacterController* controllerB = (const PhysicsCharacterController*)b.getUserData();
            if (controllerA->DoesCollideWithOtherControllers() && controllerB->DoesCollideWithOtherControllers())
            {
                const uint32_t cgA = (uint32_t)controllerA->GetCollisionGroup();
                const uint32_t cgB = (uint32_t)controllerB->GetCollisionGroup();
                const uint32_t icgA = (uint32_t)controllerA->GetInteractingCollisionGroup();
                const uint32_t icgB = (uint32_t)controllerB->GetInteractingCollisionGroup();

                return ((cgA & icgB) != 0) || ((cgB & icgA) != 0);
            }

            return false;
        }
    };

    static physx::PxCapsuleClimbingMode::Enum ToPhysXClimbingMode(CapsuleClimbingMode mode)
    {
        switch (mode)
        {
        case CapsuleClimbingMode::Easy: return physx::PxCapsuleClimbingMode::eEASY;
        case CapsuleClimbingMode::Constrained: return physx::PxCapsuleClimbingMode::eCONSTRAINED;
        default:
            EG_CORE_ASSERT(!"Unknown mode");
            return physx::PxCapsuleClimbingMode::eEASY;
        }
    }

    static CharacterControllerCollisionFlags ToCollisionFlags(physx::PxControllerCollisionFlags flags)
    {
        using namespace physx;

        CharacterControllerCollisionFlags result = CharacterControllerCollisionFlags::None;
        result |= (flags & PxControllerCollisionFlag::eCOLLISION_UP)    == PxControllerCollisionFlag::eCOLLISION_UP    ? CharacterControllerCollisionFlags::Up    : CharacterControllerCollisionFlags::None;
        result |= (flags & PxControllerCollisionFlag::eCOLLISION_DOWN)  == PxControllerCollisionFlag::eCOLLISION_DOWN  ? CharacterControllerCollisionFlags::Down  : CharacterControllerCollisionFlags::None;
        result |= (flags & PxControllerCollisionFlag::eCOLLISION_SIDES) == PxControllerCollisionFlag::eCOLLISION_SIDES ? CharacterControllerCollisionFlags::Sides : CharacterControllerCollisionFlags::None;

        return result;
    }

    static physx::PxMaterial* GetMaterial_Internal(const Ref<AssetPhysicsMaterial>& materialAsset)
    {
        const Ref<PhysicsMaterial>& material = materialAsset ? materialAsset->GetMaterial() : PhysicsEngine::GetDefaultMaterial();
        return (physx::PxMaterial*)material->GetNativeHandle();
    }

    PhysicsCharacterController::PhysicsCharacterController(const CharacterControllerComponent& component)
    {
        m_Scene = component.Parent.GetScene()->GetPhysicsScene();

        const auto& location = component.GetWorldTransform().Location;
        Recreate(physx::PxExtendedVec3(location.x, location.y, location.z));
    }

    PhysicsCharacterController::~PhysicsCharacterController()
    {
        if (!m_Scene.expired())
        {
            m_Controller->release();
            m_Controller = nullptr;
        }
    }

    void PhysicsCharacterController::SetPhysicsMaterialAsset(const Ref<AssetPhysicsMaterial>& material)
    {
        if (m_PhysicsMaterial == material)
            return;

        m_PhysicsMaterial = material;
        physx::PxMaterial* pxMaterial = GetMaterial_Internal(m_PhysicsMaterial);
        IterateShapes([pxMaterial](physx::PxShape* shape)
        {
            shape->setMaterials(&pxMaterial, 1);
        });
    }

    CharacterControllerCollisionFlags PhysicsCharacterController::Move(const glm::vec3& disp, float minDist, float elapsedTime)
    {
        const physx::PxFilterData filterData = PhysXUtils::GetPxFilterData(m_CollisionGroup, m_InteractingCollisionGroup, CollisionDetectionType::Continuous);
        PhysXQueryFilterCallback filterCallback(physx::PxQueryHitType::eBLOCK, m_CollisionGroup);
        ControllerFilterCallback cctFilterCallback;

        physx::PxControllerFilters filters;
        filters.mFilterData = &filterData;
        filters.mFilterCallback = &filterCallback;
        filters.mCCTFilterCallback = &cctFilterCallback;

        auto flags = m_Controller->move(PhysXUtils::ToPhysXVector(disp), minDist, elapsedTime, filters);
        return ToCollisionFlags(flags);
    }

    void PhysicsCharacterController::SetSlopeLimit(float degrees)
    {
        m_SlopeLimit = degrees;
        m_Controller->setSlopeLimit(glm::cos(glm::radians(m_SlopeLimit)));
    }

    void PhysicsCharacterController::SetContactOffset(float contactOffset)
    {
        m_ContactOffset = contactOffset;
        m_Controller->setContactOffset(glm::max(0.0001f, m_ContactOffset));
    }

    void PhysicsCharacterController::SetStepOffset(float stepOffset)
    {
        m_StepOffset = stepOffset;
        m_Controller->setStepOffset(m_StepOffset);
    }

    void PhysicsCharacterController::SetShapeType(CharacterControllerShape shape)
    {
        if (m_Shape == shape)
            return;

        m_Shape = shape;
        Recreate(m_Controller->getPosition());
    }

    void PhysicsCharacterController::SetCapsuleClimbingMode(CapsuleClimbingMode mode)
    {
        if (m_ClimbingMode == mode)
            return;

        m_ClimbingMode = mode;
        if (m_Shape == CharacterControllerShape::Capsule)
        {
            physx::PxCapsuleController* capsule = (physx::PxCapsuleController*)m_Controller;
            capsule->setClimbingMode(ToPhysXClimbingMode(m_ClimbingMode));
        }
    }

    void PhysicsCharacterController::SetCapsuleRadius(float radius)
    {
        if (m_Radius == radius)
            return;

        m_Radius = radius;
        if (m_Shape == CharacterControllerShape::Capsule)
        {
            physx::PxCapsuleController* capsule = (physx::PxCapsuleController*)m_Controller;
            capsule->setRadius(radius);
        }
    }

    void PhysicsCharacterController::SetCapsuleHeight(float height)
    {
        if (m_Height == height)
            return;

        m_Height = height;
        if (m_Shape == CharacterControllerShape::Capsule)
        {
            m_Controller->resize(m_Height);

            // There's also this way of changing the height, but `resize` keeps the shape at the floor level
            // physx::PxCapsuleController* capsule = (physx::PxCapsuleController*)m_Controller;
            // capsule->setHeight(m_Height);
        }
    }

    void PhysicsCharacterController::SetBoxHalfExtent(const glm::vec3& halfExtent)
    {
        if (m_HalfExtent == halfExtent)
            return;

        m_HalfExtent = halfExtent;
        if (m_Shape == CharacterControllerShape::Box)
        {
            physx::PxBoxController* box = (physx::PxBoxController*)m_Controller;
            box->setHalfSideExtent(m_HalfExtent.x);
            box->setHalfForwardExtent(m_HalfExtent.z);
            m_Controller->resize(m_HalfExtent.y);

            // There's also this way of changing the height, but `resize` keeps the shape at the floor level
            //box->setHalfHeight(m_HalfExtent.y);
        }
    }

    void PhysicsCharacterController::SetWorldLocation(const glm::vec3& location)
    {
        m_Controller->setPosition(physx::PxExtendedVec3(location.x, location.y, location.z));
    }

    glm::vec3 PhysicsCharacterController::GetWorldLocation() const
    {
        const auto res = m_Controller->getPosition();
        return glm::vec3(res.x, res.y, res.z);
    }

    glm::vec3 PhysicsCharacterController::GetFootWorldLocation() const
    {
        const auto res = m_Controller->getFootPosition();
        return glm::vec3(res.x, res.y, res.z);
    }

    void PhysicsCharacterController::Recreate(physx::PxExtendedVec3 location)
    {
        auto physicsScene = m_Scene.lock();
        if (!physicsScene)
        {
            EG_CORE_CRITICAL("Failed to obtain the physics scene ptr. Can't create a character controller");
            return;
        }

        if (m_Controller)
        {
            m_Controller->release();
            m_Controller = nullptr;
        }

        physx::PxMaterial* material = GetMaterial_Internal(m_PhysicsMaterial);

        if (m_Shape == CharacterControllerShape::Box)
        {
            physx::PxBoxControllerDesc desc{};
            desc.position = location;
            desc.material = material;
            desc.stepOffset = m_StepOffset;
            desc.contactOffset = glm::max(0.0001f, m_ContactOffset);
            desc.upDirection = physx::PxVec3(0, 1, 0);
            desc.slopeLimit = glm::cos(glm::radians(m_SlopeLimit));

            desc.halfSideExtent = m_HalfExtent.x;
            desc.halfHeight = m_HalfExtent.y;
            desc.halfForwardExtent = m_HalfExtent.z;

            m_Controller = physicsScene->CreateController(desc);
        }
        else if (m_Shape == CharacterControllerShape::Capsule)
        {
            physx::PxCapsuleControllerDesc desc{};
            desc.position = location;
            desc.material = material;
            desc.stepOffset = m_StepOffset;
            desc.contactOffset = glm::max(0.0001f, m_ContactOffset);
            desc.upDirection = physx::PxVec3(0, 1, 0);
            desc.slopeLimit = glm::cos(glm::radians(m_SlopeLimit));

            desc.radius = m_Radius;
            desc.height = m_Height;
            desc.climbingMode = ToPhysXClimbingMode(m_ClimbingMode);

            m_Controller = physicsScene->CreateController(desc);
        }
        else
        {
            EG_CORE_CRITICAL("Unknown physics character contoller type: {}", Utils::GetEnumName(m_Shape));
            EG_CORE_ASSERT(false);
        }
        m_Controller->setUserData(this);
        
        // Disable visualization
        IterateShapes([](physx::PxShape* shape)
        {
            shape->setFlag(physx::PxShapeFlag::Enum::eVISUALIZATION, false);
        });
    }
    
    void PhysicsCharacterController::IterateShapes(const std::function<void(physx::PxShape*)>& func)
    {
        physx::PxRigidDynamic* actor = m_Controller->getActor();
        const uint32_t shapesCount = actor->getNbShapes();
        std::vector<physx::PxShape*> shapes(shapesCount);

        actor->getShapes(shapes.data(), shapesCount);
        for (uint32_t i = 0; i < shapesCount; ++i)
        {
            func(shapes[i]);
        }
    }
}
