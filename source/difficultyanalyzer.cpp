//
//  difficultyanalyzer.cpp
//  DGIProject
//
//  Created by Dennis Ekström on 16/05/14.
//  Copyright (c) 2014 Dennis Ekström. All rights reserved.
//

#include "difficultyanalyzer.h"
#include "rangeterrain.h"
#include "rangedrawer.h"

//DifficultyAnalyzer gDifficultyAnalyzer;

const float p1_relative = 0.5;
const float p2_relative = 0.7;

const float DifficultyAnalyzer::IMPOSSIBLE = 400;       // Reflects a 400 yard shot, which can be considered impossible

const float defaultShotHeight   = 20;

// The following three parameters are documented to relate as 1, 1, 1/sqrt(2)
const float heightPerStep       = 1;
const float curvePerStep        = 1;
const float comboPerStep        = 1.0f / sqrtf(2.0f);

bool DifficultyAnalyzer::PathIsClear(const vec3 &tee, const vec3 & p1, const vec3 &p2, const vec3 &target) {
    return !IntersectionBetweenPoints(tee, p1) && !IntersectionBetweenPoints(p1, p2) && !IntersectionBetweenPoints(p2, target);
}

float DifficultyAnalyzer::CalculateDifficulty(const vec3 &tee, const vec3 &target, vec3 &p1_adjusted, vec3 &p2_adjusted, float &distance) {
    /*
     See attached description of golf shot modelling.
     */
    
    distance = glm::length(target - tee);
    
    // Lift tee and target just slightly to avoid immediate collisions with ground
    vec3 adjustedTee = tee + vec3(0,0.1,0);
    vec3 adjustedTarget = target + vec3(0,0.1,0);;
    
    vec3 tee_to_target_xz = adjustedTarget - adjustedTee;
    tee_to_target_xz.y = 0;
    
    float L = length(tee_to_target_xz);
    float h = adjustedTarget.y - adjustedTee.y;

    // Steps when finding difficulty
    int heightSteps = 0;
    int curveSteps = 0;
    int comboSteps = 0; // combination of height and curve
    
    // We need to adjust height if target is to high for our initial approach
    float initialHeightDifficulty = 0; // Stems from having to hit high enough to reach the green
    float adjustedHeight = defaultShotHeight; // The new height of the golf shot
    if (h + heightPerStep > defaultShotHeight) {
        heightSteps = 1 + ceil((h - defaultShotHeight) / heightPerStep); // Increase heightSteps to get future height difficulty calculations right
        adjustedHeight += heightSteps * heightPerStep;
        initialHeightDifficulty = HeightDifficulty(heightSteps * heightPerStep); // Difficulty to add to curve difficulty
    }
    
    float l = ( L * adjustedHeight ) / ( adjustedHeight - h * (1 - p2_relative) );
    
    vec3 dir = normalize(tee_to_target_xz);
    vec3 up(0, 1, 0);
    vec3 right = cross(dir, up);
    
    const vec3 p1 = adjustedTee + dir * l * p1_relative + up * adjustedHeight;
    const vec3 p2 = adjustedTee + dir * l * p2_relative + up * adjustedHeight;
    
    // These will be adjusted as difficulty increases
    p1_adjusted = p1;
    p2_adjusted = p2;
    
    // Is path clear to begin with?
    if (PathIsClear(adjustedTee, p1, p2, adjustedTarget))
        return distance + HeightDifficulty(heightSteps * heightPerStep) + initialHeightDifficulty;
    
    while (true) {
        
        // Take next step depending on which is easiest.
        float heightDifficulty = distance + HeightDifficulty((heightSteps + 1) * heightPerStep);
        float curveDifficulty  = distance + CurveDifficulty((curveSteps + 1) * curvePerStep) + initialHeightDifficulty;
        float comboDifficulty  = distance + ComboDifficulty((comboSteps + 1) * comboPerStep, (comboSteps + 1) * comboPerStep);
        
        // If difficulty exceeded IMPOSSIBLE, indicate this bu returning -1
        if (heightDifficulty > IMPOSSIBLE && curveDifficulty > IMPOSSIBLE && comboDifficulty > IMPOSSIBLE)
            return -1;
        
        // Which approach offers the easiest shot?
        float minDifficulty = std::min(heightDifficulty, std::min(curveDifficulty, comboDifficulty));
        
        vec3 diff;
        if (minDifficulty == heightDifficulty) {
            
            heightSteps += 1;
            
            diff = up * (heightSteps * heightPerStep);
            p1_adjusted = p1 + diff;
            p2_adjusted = p2 + diff;
            if (PathIsClear(adjustedTee, p1_adjusted, p2_adjusted, adjustedTarget))
                return heightDifficulty;
            
        } else if (minDifficulty == curveDifficulty) {
            
            curveSteps += 1;
            
            // Try curve to the right (path curves left)
            diff = right * (curveSteps * curvePerStep);
            p1_adjusted = p1 + diff;
            p2_adjusted = p2 + diff;
            if (PathIsClear(adjustedTee, p1_adjusted, p2_adjusted, adjustedTarget))
                return curveDifficulty;
            
            // Try curve to the left (path curves right)
            diff = -right * (curveSteps * curvePerStep);
            p1_adjusted = p1 + diff;
            p2_adjusted = p2 + diff;
            if (PathIsClear(adjustedTee, p1_adjusted, p2_adjusted, adjustedTarget))
                return curveDifficulty;
            
            
            
        } else if (minDifficulty == comboDifficulty) {
            
            comboSteps += 1;
            
            // Try curve to the right (path curves left)
            diff = up * (comboSteps * heightPerStep) + right * (comboSteps * curvePerStep);
            p1_adjusted = p1 + diff;
            p2_adjusted = p2 + diff;
            if (PathIsClear(adjustedTee, p1_adjusted, p2_adjusted, adjustedTarget))
                return comboDifficulty;
            
            // Try curve to the left (path curves right)
            diff = up * (comboSteps * heightPerStep) - right * (comboSteps * curvePerStep);
            p1_adjusted = p1 + diff;
            p2_adjusted = p2 + diff;
            if (PathIsClear(adjustedTee, p1_adjusted, p2_adjusted, adjustedTarget))
                return comboDifficulty;
        }
    }
}

bool DifficultyAnalyzer::IntersectionBetweenPoints(const vec3 &start, const vec3 &end) {
    
    const vec3 dir = end - start;
    
    // iterate through all terrain vertices and check for intersection
    for (int i = 0; i < (X_INTERVAL-1) * (Y_INTERVAL-1) * FLOATS_PER_TRIANGLE * 2 - 36 ; i += 36) {
        
        vec3 v0 = vec3(gTerrain.vertexData[i], gTerrain.vertexData[i+1], gTerrain.vertexData[i+2]);
        vec3 v1 = vec3(gTerrain.vertexData[i+12],gTerrain.vertexData[i+13],gTerrain.vertexData[i+14]);
        vec3 v2 = vec3(gTerrain.vertexData[i+24],gTerrain.vertexData[i+25],gTerrain.vertexData[i+26]);
        
        vec3 e1 = v1 - v0;
        vec3 e2 = v2 - v0;
        vec3 b = start - v0;
        mat3 A( -dir, e1, e2 );
        vec3 x = glm::inverse( A ) * b; // x = (t, u, v)
        
        if (x.x > 0 && x.x < 1 && x.y >= 0 && x.z >= 0 && x.y + x.z <= 1) {
//            cout << "intersection t: " << x.x << endl;
            return true;
        }
    }
    
    return false;
}

bool DifficultyAnalyzer::ClosestIntersection(vec3 start, vec3 dir, Intersection& closestIntersection) {
    
    float closest_t = std::numeric_limits<float>::max();
    float closest_index = -1;
    
    // iterate through all terrain vertices and check for intersection
    for (int i = 0; i < (X_INTERVAL-1) * (Y_INTERVAL-1) * FLOATS_PER_TRIANGLE * 2 - 36 ; i += 36) {
        
        vec3 v0 = vec3(gTerrain.vertexData[i], gTerrain.vertexData[i+1], gTerrain.vertexData[i+2]);
        vec3 v1 = vec3(gTerrain.vertexData[i+12],gTerrain.vertexData[i+13],gTerrain.vertexData[i+14]);
        vec3 v2 = vec3(gTerrain.vertexData[i+24],gTerrain.vertexData[i+25],gTerrain.vertexData[i+26]);

        vec3 e1 = v1 - v0;
        vec3 e2 = v2 - v0;
        vec3 b = start - v0;
        mat3 A( -dir, e1, e2 );
        vec3 x = glm::inverse( A ) * b; // x = (t, u, v)
        
        if (x.x < closest_t && x.x > 0 && x.y >= 0 && x.z >= 0 && x.y + x.z <= 1) {
            closest_t = x.x;
            closest_index = i;
        }
    }
    
    if (closest_index >= 0) {
        closestIntersection.position = start + closest_t * dir;
        closestIntersection.distance = closest_t;
        return true;
    }
    
    return false;
}