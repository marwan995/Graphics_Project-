#pragma once

#include "projectile.hpp"
#include "explosion.hpp"

namespace our{
    class RocketBullet : public Projectile
    {
    private:
        World* world;

    public:
        // initialize the bullet attributes
        RocketBullet(const float (&cameraPosition)[3], const float (&cameraRotation)[3], const float (&cameraRotationProjection)[3], World* currentWorld, bool friendlyFire)
        {
            mesh = "laser";
            material = "laser";
            scale[0] = 0.4;
            scale[1] = 0.4;
            scale[2] = 0.03;
            
            world = currentWorld;
            speed = 7;
            radius = 0.3;
            damage = 700;
            isFriendly = friendlyFire;

            Projectile::copyArr(position, cameraPosition);
            Projectile::copyArr(rotation, cameraRotation);
            Projectile::copyArr(linearVelocity, cameraRotationProjection, speed);
        };

        // spawn the bullet
        void shoot() override{
            // create the bullet entity in the world
            Entity* rocketBulletEntity = world->add();

            // get the json object representing bullet entity
            nlohmann::json bulletData = Projectile::spawn();

            // deserialize the entity data to render it and add bullet data
            rocketBulletEntity->deserialize(bulletData);
            
            // get the collider component
            ColliderComponent* rocketBulletCollider = rocketBulletEntity->getComponent<ColliderComponent>();
            
            // set collider attributes
            rocketBulletCollider->setEntity(rocketBulletEntity);

            // push the entity to the colliders array
            world->addDynamicEntity(rocketBulletEntity);

            // add the laser bullet component to the entity to use the hit function later
            rocketBulletEntity->appendComponent<RocketBullet>(this);
        };

        // returns true of the hit entity's health is depleted
        bool hit(World* world, Entity* hitEntity) override
        {
            // copy the rocket's position
            glm::vec3 bulletPosition = getOwner()->localTransform.position;
            float explosionPosition[3] = {bulletPosition.x, bulletPosition.y, bulletPosition.z};
            
            // create an explosion when a collision happens
            Explosion* explosion = new Explosion(explosionPosition, world, true);
            explosion->isFriendly = true;
            explosion->shoot();

            if (hitEntity->health != FLT_MAX)           // not a static object (a wall for example)
            {    
                hitEntity->health -= damage;            // decrease enemy's health
                if (hitEntity->health <= 0)             // if no remaining health remove the enemy
                {
                    world->markForRemoval(hitEntity);
                    return true;
                }
            }
            return false;
        };
    };    
}