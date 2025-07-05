#pragma once

#include "CoreMinimal.h"

// A node in the Quadtree
struct FQuadtreeNode
{
	FQuadtreeNode(const FBox2D& InBounds) : Bounds(InBounds) {}

	FBox2D Bounds;
	TArray<FIntPoint> Points;
	TUniquePtr<FQuadtreeNode> Children[4];
};

// The Quadtree itself
class FQuadtree
{
public:
	FQuadtree(const FBox2D& InBounds, int32 InMaxPointsPerNode = 4);

	void Insert(const FIntPoint& Point);
	bool FindNearest(const FIntPoint& SearchPoint, float MaxSearchRadius, FIntPoint& OutNearestPoint) const;

private:
	void Subdivide(FQuadtreeNode* Node);
	int32 GetChildIndexForPoint(const FQuadtreeNode* Node, const FIntPoint& Point) const;
	void FindNearestRecursive(const FQuadtreeNode* Node, const FIntPoint& SearchPoint, float& MinDistanceSq, FIntPoint& OutNearestPoint) const;

	TUniquePtr<FQuadtreeNode> Root;
	int32 MaxPointsPerNode;
};
