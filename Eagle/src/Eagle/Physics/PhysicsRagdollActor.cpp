#include "egpch.h"
#include "PhysicsActor.h"
#include "PhysicsRagdollActor.h"
#include "PhysXInternal.h"
#include "PhysicsUtils.h"

#include "Eagle/Components/Components.h"
#include "Eagle/Animation/AnimationSystem.h"

namespace Eagle
{
    static physx::PxShape* CreateShape(physx::PxPhysics& physics, SkeletalRagdollBones::UserSettings::ShapeType type, const glm::vec3& scale, float radius, float halfHeight, const physx::PxMaterial* material)
    {
        using namespace physx;
        switch (type)
        {
            case SkeletalRagdollBones::UserSettings::ShapeType::Box:
                return physics.createShape(PxBoxGeometry(halfHeight * scale.x, halfHeight * scale.y, halfHeight * scale.z), *material, true);
            case SkeletalRagdollBones::UserSettings::ShapeType::Sphere:
                return physics.createShape(PxSphereGeometry(radius * scale.x), *material, true);
            case SkeletalRagdollBones::UserSettings::ShapeType::Capsule:
                return physics.createShape(PxCapsuleGeometry(radius * scale.x, halfHeight * scale.y), *material, true);
            default:
                EG_CORE_ASSERT(false);
                return physics.createShape(PxCapsuleGeometry(radius * scale.x, halfHeight * scale.y), *material, true);
        }
    }

    // TODO: group args
    static void CreateArticulationChain(const SkeletalRagdollBones& bone, const SkeletalPose& currentPose, const BonesMap& boneMap, physx::PxScene* scene, PhysicsRagdollActor::BoneData& physicsBoneData, PhysicsActorPayload& payload,
        const PhysicsSettings& settings, const glm::mat4& worldTransform, const glm::mat4& compWorldTrInv, float twist, float swing, const physx::PxVec3& linearVelocity,
        const physx::PxVec3& angularVelocity, std::unordered_map<std::string, physx::PxRigidDynamic*>& ragdollBonesMap, physx::PxRigidDynamic* parentBody = nullptr)
    {
        using namespace physx;

        // TODO: Expose
        static physx::PxFilterData s_FilterData(1, 1, 0, 0);
        auto& physics = PhysXInternal::GetPhysics();
        const auto it = currentPose.Bones.find(bone.Name);

        auto itParentBoneMap = boneMap.find(bone.Name);
        const bool bValidBone = itParentBoneMap != boneMap.end();
        const glm::mat4 boneOffset = bValidBone ? itParentBoneMap->second.Offset : glm::mat4(1.f);

        const glm::mat4 boneWorldTransform = worldTransform * bone.LocalTransform;
        const PxTransform transform = PhysXUtils::ToPhysXTranform(boneWorldTransform);

        const glm::vec3 locationOffset = bone.Settings.UserOffset.Location;
        glm::vec3 parentPos = PhysXUtils::FromPhysXVector(transform.p) + locationOffset;
        const PxTransform jointTransform{ transform.p, PhysXUtils::ToPhysXQuat(glm::quat_cast(boneOffset)) };

        {
            constexpr float minRadius = 0.005f;
            constexpr float maxRadius = 0.05f;

            const glm::vec3 aabbWorld = glm::vec3(worldTransform * glm::vec4(bone.AABB.Center(), 1.f)) + locationOffset;
            glm::vec3 childPos = parentPos + (aabbWorld - parentPos) * 2.f;

            const float len = glm::length(parentPos - childPos) * 0.95f; // shorten to reduce overlap
            const float radius = glm::clamp(len * 0.1f, minRadius, maxRadius);
            const float lenMinus2r = len - 2.0f * radius;
            const float halfHeight = lenMinus2r * 0.5f;
            const glm::vec3 bodyLocation = (parentPos + childPos) * 0.5f;

            // Rotate around `bodyLocation`
            parentPos = bodyLocation + glm::rotate(bone.Settings.UserOffset.Rotation.GetQuat(), parentPos - bodyLocation);
            childPos = bodyLocation + glm::rotate(bone.Settings.UserOffset.Rotation.GetQuat(), childPos - bodyLocation);

            const glm::mat4 lookAt = glm::inverse(glm::lookAt(bodyLocation, childPos, glm::vec3(0, 1, 0)));
            const bool bAnyNan = glm::any(glm::isnan(lookAt[0])) || glm::any(glm::isnan(lookAt[1])) || glm::any(glm::isnan(lookAt[2])) || glm::any(glm::isnan(lookAt[3]));
            const PxQuat q = bAnyNan ? PxQuat(PxIdentity) : PhysXUtils::ToPhysXQuat(Math::DecomposeTransformMatrix(lookAt).Rotation.GetQuat());

            PxRigidDynamic* body = physics.createRigidDynamic(PxTransform(PhysXUtils::ToPhysXVector(bodyLocation), q));
            body->setSolverIterationCounts(settings.SolverIterations, settings.SolverVelocityIterations);
            body->userData = &payload;
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
                parentBody->userData = &payload;
                parentBody->setSolverIterationCounts(settings.SolverIterations, settings.SolverVelocityIterations);
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
            shape->setSimulationFilterData(s_FilterData);
            PxTransform local(PhysXUtils::ToPhysXQuat(glm::quat_cast(rot)));
            shape->setLocalPose(local);
            body->attachShape(*shape);
            if (bValidBone)
            {
                body->setLinearVelocity(linearVelocity);
                body->setAngularVelocity(angularVelocity);
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
                CreateArticulationChain(child, currentPose, boneMap, scene, childData, payload, settings, worldTransform, compWorldTrInv, twist, swing, linearVelocity, angularVelocity, ragdollBonesMap, body);
        }
    }

    static void UpdatePose(const PhysicsRagdollActor::BoneData& node, SkeletalPose& pose, const glm::mat4& origBaseWorldTrInv)
    {
        if (node.bValidBone)
        {
            Transform transform = PhysXUtils::FromPhysXTransform(node.Body->getGlobalPose());
            glm::mat4 currentWorldTr = origBaseWorldTrInv * Math::ToTransformMatrix(transform);
            glm::mat4 offsetTr = currentWorldTr * node.OriginalBodyTrInv;
            pose.Bones[node.Name] = Math::DecomposeTransformMatrix(offsetTr * node.BoneWorldTr);
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

    PhysicsRagdollActor::PhysicsRagdollActor(Entity entity, physx::PxScene* scene, const PhysicsSettings& settings)
		: m_Entity(entity), m_Scene(scene), m_Settings(settings)
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
        m_Payload.Ptr = this;
        m_Payload.bRagdoll = true;

        const glm::mat4 worldTransform = Math::ToTransformMatrix(skeletalComp.GetWorldTransform());
        m_OriginalTransformInv = glm::inverse(worldTransform);
        if (skeletalComp.LastPose.Bones.empty())
        {
            // If LastPose is empty, fill it with base pose data
            const glm::mat4 rootTransform = glm::mat4(1.f);
            AnimationSystem::FinalizePose(skeletalComp.LastPose, rootNode, rootTransform);
        }

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
        
		CreateArticulationChain(mesh->GetRagdollRoot(), skeletalComp.LastPose, meshInfo.BoneInfoMap, m_Scene, m_Root, m_Payload, m_Settings, worldTransform, m_OriginalTransformInv,
            glm::radians(twist), glm::radians(swing), linearVelocity, angularVelocity, m_BonesMap);
	}

    PhysicsRagdollActor::~PhysicsRagdollActor()
    {
        const bool bExists = m_Root.Body != nullptr;
        if (bExists && m_Entity.HasComponent<SkeletalMeshComponent>())
        {
            m_Entity.GetComponent<SkeletalMeshComponent>().LastPose.Reset();
        }
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
}
