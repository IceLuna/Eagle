#include "egpch.h"
#include "PhysicsActor.h"
#include "PhysicsRagdollActor.h"
#include "PhysXInternal.h"
#include "PhysicsUtils.h"

#include "Eagle/Components/Components.h"
#include "Eagle/Animation/AnimationSystem.h"

namespace Eagle
{
    static physx::PxShape* CreateShape(physx::PxPhysics& physics, SkeletalRagdollBone::UserSettings::ShapeType type, const glm::vec3& scale, float radius, float halfHeight, const physx::PxMaterial* material)
    {
        using namespace physx;
        switch (type)
        {
            case SkeletalRagdollBone::UserSettings::ShapeType::Box:
                return physics.createShape(PxBoxGeometry(halfHeight * scale.x, halfHeight * scale.y, halfHeight * scale.z), *material, true);
            case SkeletalRagdollBone::UserSettings::ShapeType::Sphere:
                return physics.createShape(PxSphereGeometry(radius * scale.x), *material, true);
            case SkeletalRagdollBone::UserSettings::ShapeType::Capsule:
                return physics.createShape(PxCapsuleGeometry(radius * scale.x, halfHeight * scale.y), *material, true);
            default:
                EG_CORE_ASSERT(false);
                return physics.createShape(PxCapsuleGeometry(radius * scale.x, halfHeight * scale.y), *material, true);
        }
    }

    // TODO: group args
    static void CreateArticulationChain(const SkeletalRagdollBone& bone, const std::unordered_map<std::string_view, glm::mat4>& boneTransforms, const BonesMap& boneMap, physx::PxScene* scene, PhysicsRagdollActor::BoneData& physicsBoneData, void* userData,
        const glm::mat4& worldTransform, const glm::mat4& compWorldTrInv, float twist, float swing, const physx::PxFilterData& filterData, const physx::PxVec3& linearVelocity,
        const physx::PxVec3& angularVelocity, ankerl::unordered_dense::map<std::string, physx::PxRigidDynamic*>& ragdollBonesMap, physx::PxRigidDynamic* parentBody = nullptr)
    {
        using namespace physx;

        auto& physics = PhysXInternal::GetPhysics();

        auto itParentBoneMap = boneMap.find(bone.Name);
        const bool bValidBone = itParentBoneMap != boneMap.end();
        const glm::mat4 boneOffset = bValidBone ? itParentBoneMap->second.Offset : glm::mat4(1.f);

        const glm::mat4& boneWorldTransform = boneTransforms.at(bone.Name);
        const PxTransform transform = PhysXUtils::ToPhysXTranform(boneWorldTransform);

        const glm::vec3& locationOffset = bone.Settings.UserOffset.Location;
        glm::vec3 parentPos = PhysXUtils::FromPhysXVector(transform.p) + locationOffset;
        const PxTransform jointTransform{ transform.p, PhysXUtils::ToPhysXQuat(glm::quat_cast(boneOffset)) };

        {
            constexpr float minHalfHeight = 0.005f;
            constexpr float minRadius = 0.005f;
            constexpr float maxRadius = 0.05f;

            const glm::vec3 center = AABB::Transformed(bone.AABB, boneWorldTransform).Center();
            const glm::vec3 aabbWorld = center + locationOffset;
            glm::vec3 childPos = parentPos + (aabbWorld - parentPos) * 2.f;

            const float len = glm::length(parentPos - childPos) * 0.95f; // shorten to reduce overlap
            const float radius = glm::clamp(len * 0.1f, minRadius, maxRadius);
            const float lenMinus2r = len - 2.0f * radius;
            const float halfHeight = glm::max(lenMinus2r * 0.5f, minHalfHeight);
            const glm::vec3 bodyLocation = (parentPos + childPos) * 0.5f;

            // Rotate around `bodyLocation`
            parentPos = bodyLocation + glm::rotate(bone.Settings.UserOffset.Rotation.GetQuat(), parentPos - bodyLocation);
            childPos = bodyLocation + glm::rotate(bone.Settings.UserOffset.Rotation.GetQuat(), childPos - bodyLocation);

            const glm::mat4 lookAt = glm::inverse(glm::lookAt(bodyLocation, childPos, glm::vec3(0, 1, 0)));
            const bool bAnyNan = glm::any(glm::isnan(lookAt[0])) || glm::any(glm::isnan(lookAt[1])) || glm::any(glm::isnan(lookAt[2])) || glm::any(glm::isnan(lookAt[3]));
            const PxQuat q = bAnyNan ? PxQuat(PxIdentity) : PhysXUtils::ToPhysXQuat(Math::DecomposeTransformMatrix(lookAt).Rotation.GetQuat());

            PxRigidDynamic* body = physics.createRigidDynamic(PxTransform(PhysXUtils::ToPhysXVector(bodyLocation), q));
            body->setSolverIterationCounts(bone.Settings.PositionSolverIterations, bone.Settings.VelocitySolverIterations);
            body->userData = userData;
            scene->addActor(*body);
            if (!parentBody)
            {
                // Create a parent to hold together all other bodies
                parentBody = PxCloneDynamic(physics, body->getGlobalPose(), *body);
                scene->addActor(*parentBody);
                physicsBoneData.Body = parentBody;
                physicsBoneData.Name = bone.Name;
                physicsBoneData.BoneWorldTr = compWorldTrInv * boneWorldTransform;
                physicsBoneData.OriginalBodyTrInv = glm::inverse(Math::ToTransformMatrix(PhysXUtils::FromPhysXTransform(parentBody->getGlobalPose())));
                physicsBoneData.bValidBone = bValidBone;
                parentBody->userData = userData;
            }

            // Setup collider
            const glm::mat4 rot = glm::rotate(glm::mat4(1.0f), PxHalfPi, glm::vec3(0.0f, 1.0f, 0.0f));
            const auto& material = bone.Settings.Material ? bone.Settings.Material->GetMaterial() : PhysicsEngine::GetDefaultMaterial();
            const PxMaterial* physxMaterial = (const PxMaterial*)material->GetNativeHandle();
            PxShape* shape = CreateShape(physics, bone.Settings.Shape, bone.Settings.UserOffset.Scale3D, radius, halfHeight, physxMaterial);
            if (bValidBone == false)
            {
                shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false); // Disable collision
                body->setActorFlag(PxActorFlag::eDISABLE_SIMULATION, true); // Disable simulation
            }
            shape->setFlag(physx::PxShapeFlag::Enum::eVISUALIZATION, false);
            shape->setSimulationFilterData(filterData);
            PxTransform local(PhysXUtils::ToPhysXQuat(glm::quat_cast(rot)));
            shape->setLocalPose(local);
            body->attachShape(*shape);
            if (bValidBone)
            {
                body->setLinearVelocity(linearVelocity);
                body->setAngularVelocity(angularVelocity);
                body->setActorFlag(PxActorFlag::eDISABLE_SIMULATION, !bone.Settings.bEnableSimulation);
                shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, bone.Settings.bEnableSimulation);
            }
            body->setLinearDamping(bone.Settings.LinearDamping);
            body->setAngularDamping(bone.Settings.AngularDamping);
            body->setMass(bone.Settings.Mass);

            // Setup Joint
            PxD6Joint* joint = PxD6JointCreate(physics,
                parentBody,
                parentBody->getGlobalPose().transformInv(jointTransform),
                body,
                body->getGlobalPose().transformInv(jointTransform));

            //joint->setConstraintFlag(PxConstraintFlag::eVISUALIZATION, true);
            joint->setMotion(physx::PxD6Axis::eSWING1, physx::PxD6Motion::eLIMITED);
            joint->setMotion(physx::PxD6Axis::eSWING2, physx::PxD6Motion::eLIMITED);
            joint->setMotion(physx::PxD6Axis::eTWIST, physx::PxD6Motion::eLIMITED);
            joint->setSwingLimit(physx::PxJointLimitCone(swing, swing));
            joint->setTwistLimit(physx::PxJointAngularLimitPair(-twist, twist));

            auto& childData = physicsBoneData.Children.emplace_back();
            childData.Shape = shape;
            childData.bValidBone = bValidBone;
            childData.Body = body;
            childData.Joint = joint;
            childData.Name = bone.Name;
            childData.BoneWorldTr = compWorldTrInv * boneWorldTransform;
            childData.OriginalBodyTrInv = glm::inverse(compWorldTrInv * Math::ToTransformMatrix(PhysXUtils::FromPhysXTransform(body->getGlobalPose())));
            ragdollBonesMap[bone.Name] = body;

            for (const auto& child : bone.Children)
                CreateArticulationChain(child, boneTransforms, boneMap, scene, childData, userData, worldTransform, compWorldTrInv, twist, swing, filterData, linearVelocity, angularVelocity, ragdollBonesMap, body);
        }
    }

    static void UpdatePose(const PhysicsRagdollActor::BoneData& node, SkeletalPose& pose, const glm::mat4& origBaseWorldTrInv)
    {
        if (node.bValidBone)
        {
            Transform transform = PhysXUtils::FromPhysXTransform(node.Body->getGlobalPose());
            glm::mat4 currentWorldTr = origBaseWorldTrInv * Math::ToTransformMatrix(transform);
            glm::mat4 offsetTr = currentWorldTr * node.OriginalBodyTrInv;
            const size_t nameHash = Utils::CalculateBoneNameHash(node.Name);
            pose.Bones[nameHash] = Math::DecomposeTransformMatrix(offsetTr * node.BoneWorldTr);
        }

        for (const auto& child : node.Children)
        {
            UpdatePose(child, pose, origBaseWorldTrInv);
        }
    }

    static void Release(PhysicsRagdollActor::BoneData& node)
    {
        if (node.Shape)
        {
            // No need to release, it's a exclusive shape
            //node.Shape->release();
            node.Shape = nullptr;
        }
        if (node.Body)
        {
            node.Body->release();
            node.Body = nullptr;
        }
        if (node.Joint)
        {
            node.Joint->release();
            node.Joint = nullptr;
        }

        for (auto& child : node.Children)
        {
            Release(child);
        }
        node = {};
    }

    static void SetShowCollision_Internal(const PhysicsRagdollActor::BoneData& node, bool bShow)
    {
        if (node.Shape && node.bValidBone)
        {
            node.Shape->setFlag(physx::PxShapeFlag::Enum::eVISUALIZATION, bShow);
        }

        for (const auto& child : node.Children)
            SetShowCollision_Internal(child, bShow);
    }

    static void GatherTransforms(const SkeletalPose& pose, const BoneNode& node, const glm::mat4& parentTransform, std::unordered_map<std::string_view, glm::mat4>& boneTransforms)
    {
        glm::mat4 globalTransformation;
        if (auto it = pose.FindBone(node.GetNameHash()); it != pose.Bones.end())
        {
            const auto& bone = it->second;
            const glm::mat4 boneTransform = Math::ToTransformMatrix(bone);
            globalTransformation = parentTransform * boneTransform;
        }
        else
            globalTransformation = parentTransform * node.Transformation;

        boneTransforms[node.GetName()] = globalTransformation;

        for (auto& child : node.Children)
            GatherTransforms(pose, child, globalTransformation, boneTransforms);
    }

    PhysicsRagdollActor::PhysicsRagdollActor(Entity entity, physx::PxScene* scene)
		: PhysicsActorBase(entity), m_Scene(scene)
	{
        if (!m_Entity.HasComponent<SkeletalMeshComponent>())
        {
            EG_CORE_ERROR("Failed to create a ragdoll actor. SkeletalMeshComponent is not present (Entity: {})", m_Entity.GetName());
            return;
        }

		auto& skeletalComp = m_Entity.GetComponent<SkeletalMeshComponent>();
		const auto& asset = skeletalComp.GetMeshAsset();
		if (!asset)
		{
            EG_CORE_ERROR("Failed to create a ragdoll actor. Skeletal mesh asset is not present (Entity: {})", m_Entity.GetName());
			return;
		}

		auto& physics = PhysXInternal::GetPhysics();
        const auto& mesh = asset->GetMesh();
		const auto& meshInfo = mesh->GetSkeletalMeshInfo();
        const float twist = mesh->GetRagdollMaxTwist();
        const float swing = mesh->GetRagdollMaxSwing();
		auto& rootNode = meshInfo.RootBone;
        void* userData = this;
        const physx::PxFilterData filterData = PhysXUtils::GetPxFilterData(mesh->GetCollisionGroup(), mesh->GetInteractingCollisionGroup(), mesh->GetCollisionDetectionType());

        const glm::mat4 worldTransform = Math::ToTransformMatrix(skeletalComp.GetWorldTransform());
        m_OriginalTransformInv = glm::inverse(worldTransform);

        physx::PxVec3 linearVelocity(0.f);
        physx::PxVec3 angularVelocity(0.f);
        if (const auto& actor = m_Entity.GetPhysicsActor())
        {
            if (actor->IsDynamic())
            {
                linearVelocity = PhysXUtils::ToPhysXVector(actor->GetLinearVelocity());
                angularVelocity = PhysXUtils::ToPhysXVector(actor->GetAngularVelocity());
            }
        }

        std::unordered_map<std::string_view, glm::mat4> boneTransforms;
        GatherTransforms(skeletalComp.LastPose, meshInfo.RootBone, worldTransform, boneTransforms);
		CreateArticulationChain(mesh->GetRagdollRoot(), boneTransforms, meshInfo.GetBoneInfoMap(), m_Scene, m_Root, userData, worldTransform, m_OriginalTransformInv,
            glm::radians(twist), glm::radians(swing), filterData, linearVelocity, angularVelocity, m_BonesMap);
        m_RigidActor = m_Root.Body;
	}

    PhysicsRagdollActor::~PhysicsRagdollActor()
    {
        Release(m_Root);
    }
    
    void PhysicsRagdollActor::SynchronizeTransform()
    {
        const bool bInvalid = m_Root.Body == nullptr;
        if (bInvalid)
            return;

        auto& skeletalComp = m_Entity.GetComponent<SkeletalMeshComponent>();
        auto& lastPose = skeletalComp.LastPose;
        lastPose.Reset();

        UpdatePose(m_Root, lastPose, m_OriginalTransformInv);
        m_bDirtyTransform = false;
    }
    
    void PhysicsRagdollActor::SetShowCollision(bool bShowCollision)
    {
        m_bShowCollision = bShowCollision;
        SetShowCollision_Internal(m_Root, bShowCollision);
    }
    
    Transform PhysicsRagdollActor::GetBoneWorldTransform(const std::string& boneName) const
    {
        if (auto it = m_BonesMap.find(boneName); it != m_BonesMap.end())
            return PhysXUtils::FromPhysXTransform(it->second->getGlobalPose());
        return {};
    }

    Transform PhysicsRagdollActor::GetRootBoneWorldTransform() const
    {
        return PhysXUtils::FromPhysXTransform(m_Root.Body->getGlobalPose());
    }

    void PhysicsRagdollActor::SetLinearVelocity(const glm::vec3& velocity, bool bApplyToRootOnly)
    {
        const auto pxVel = PhysXUtils::ToPhysXVector(velocity);
        m_Root.Body->setLinearVelocity(pxVel);
        if (!bApplyToRootOnly)
        {
            for (auto& [_, body] : m_BonesMap)
                body->setLinearVelocity(pxVel);
        }
    }

    void PhysicsRagdollActor::SetAngularVelocity(const glm::vec3& velocity, bool bApplyToRootOnly)
    {
        const auto pxVel = PhysXUtils::ToPhysXVector(velocity);
        m_Root.Body->setAngularVelocity(pxVel);
        if (!bApplyToRootOnly)
        {
            for (auto& [_, body] : m_BonesMap)
                body->setAngularVelocity(pxVel);
        }
    }

    void PhysicsRagdollActor::AddForce(const glm::vec3& force, ForceMode forceMode, bool bApplyToRootOnly)
    {
        const auto pxForce = PhysXUtils::ToPhysXVector(force);
        m_Root.Body->addForce(pxForce, (physx::PxForceMode::Enum)forceMode);
        if (!bApplyToRootOnly)
        {
            for (auto& [_, body] : m_BonesMap)
                body->addForce(pxForce, (physx::PxForceMode::Enum)forceMode);
        }
    }

    void PhysicsRagdollActor::AddForceAtLocation(const glm::vec3& location, const glm::vec3& force, ForceMode forceMode, bool bApplyToRootOnly)
    {
        const auto pxLocation = PhysXUtils::ToPhysXVector(location);
        const auto pxForce = PhysXUtils::ToPhysXVector(force);

        physx::PxRigidBodyExt::addForceAtPos(*m_Root.Body, pxForce, pxLocation, (physx::PxForceMode::Enum)forceMode);
        if (!bApplyToRootOnly)
        {
            for (auto& [_, body] : m_BonesMap)
                physx::PxRigidBodyExt::addForceAtPos(*body, pxForce, pxLocation, (physx::PxForceMode::Enum)forceMode);
        }
    }

    void PhysicsRagdollActor::AddTorque(const glm::vec3& torque, ForceMode forceMode, bool bApplyToRootOnly)
    {
        const auto pxTorque = PhysXUtils::ToPhysXVector(torque);
        m_Root.Body->addTorque(pxTorque, (physx::PxForceMode::Enum)forceMode);
        if (!bApplyToRootOnly)
        {
            for (auto& [_, body] : m_BonesMap)
                body->addTorque(pxTorque, (physx::PxForceMode::Enum)forceMode);
        }
    }
    
    glm::vec3 PhysicsRagdollActor::GetLinearVelocity() const
    {
        return PhysXUtils::FromPhysXVector(m_Root.Body->getLinearVelocity());
    }
    
    glm::vec3 PhysicsRagdollActor::GetAngularVelocity() const
    {
        return PhysXUtils::FromPhysXVector(m_Root.Body->getAngularVelocity());
    }

    void PhysicsRagdollActor::SetBoneLinearVelocity(const std::string& boneName, const glm::vec3& velocity)
    {
        if (auto it = m_BonesMap.find(boneName); it != m_BonesMap.end())
            it->second->setLinearVelocity(PhysXUtils::ToPhysXVector(velocity));
    }
    
    glm::vec3 PhysicsRagdollActor::GetBoneLinearVelocity(const std::string& boneName) const
    {
        if (auto it = m_BonesMap.find(boneName); it != m_BonesMap.end())
            return PhysXUtils::FromPhysXVector(it->second->getLinearVelocity());
        return glm::vec3(0);
    }
    
    void PhysicsRagdollActor::SetBoneAngularVelocity(const std::string& boneName, const glm::vec3& velocity)
    {
        if (auto it = m_BonesMap.find(boneName); it != m_BonesMap.end())
            it->second->setAngularVelocity(PhysXUtils::ToPhysXVector(velocity));
    }
    
    glm::vec3 PhysicsRagdollActor::GetBoneAngularVelocity(const std::string& boneName) const
    {
        if (auto it = m_BonesMap.find(boneName); it != m_BonesMap.end())
            return PhysXUtils::FromPhysXVector(it->second->getAngularVelocity());
        return glm::vec3(0);
    }

    void PhysicsRagdollActor::AddBoneForce(const std::string& boneName, const glm::vec3& force, ForceMode forceMode)
    {
        if (auto it = m_BonesMap.find(boneName); it != m_BonesMap.end())
            it->second->addForce(PhysXUtils::ToPhysXVector(force), (physx::PxForceMode::Enum)forceMode);
    }

    void PhysicsRagdollActor::AddBoneForceAtLocation(const std::string& boneName, const glm::vec3& location, const glm::vec3& force, ForceMode forceMode)
    {
        if (auto it = m_BonesMap.find(boneName); it != m_BonesMap.end())
            physx::PxRigidBodyExt::addForceAtPos(*(it->second), PhysXUtils::ToPhysXVector(force), PhysXUtils::ToPhysXVector(location), (physx::PxForceMode::Enum)forceMode);
    }

    void PhysicsRagdollActor::AddBoneTorque(const std::string& boneName, const glm::vec3& torque, ForceMode forceMode)
    {
        if (auto it = m_BonesMap.find(boneName); it != m_BonesMap.end())
            it->second->addTorque(PhysXUtils::ToPhysXVector(torque), (physx::PxForceMode::Enum)forceMode);
    }
    
    void PhysicsRagdollActor::PutToSleep()
    {
        m_Root.Body->putToSleep();
        for (auto& [_, body] : m_BonesMap)
        {
            body->putToSleep();
        }
    }
    
    void PhysicsRagdollActor::WakeUp()
    {
        m_Root.Body->wakeUp();
        for (auto& [_, body] : m_BonesMap)
        {
            body->wakeUp();
        }
    }
}
