#include "Spatial/Quadtree.h"

FQuadtree::FQuadtree(const FBox2D& InBounds, int32 InMaxPointsPerNode)
	: MaxPointsPerNode(InMaxPointsPerNode)
{
	Root = MakeUnique<FQuadtreeNode>(InBounds);
}

void FQuadtree::Insert(const FIntPoint& Point)
{
	FQuadtreeNode* Node = Root.Get();
	while (Node->Children[0])
	{
		Node = Node->Children[GetChildIndexForPoint(Node, Point)].Get();
	}

	Node->Points.Add(Point);

	if (Node->Points.Num() > MaxPointsPerNode)
	{
		Subdivide(Node);
	}
}

bool FQuadtree::Remove(const FIntPoint& Point)
{
	return RemoveFromNode(Root.Get(), Point);
}

bool FQuadtree::RemoveFromNode(FQuadtreeNode* Node, const FIntPoint& Point)
{
	if (!Node->Bounds.IsInside(FVector2D(Point.X, Point.Y)))
	{
		return false;
	}

	// If we are a leaf node, try to remove the point directly
	if (Node->Children[0] == nullptr)
	{
		int32 RemovedCount = Node->Points.Remove(Point);
		return RemovedCount > 0;
	}

	// Otherwise, recurse down to the correct child
	int32 ChildIndex = GetChildIndexForPoint(Node, Point);
	if (Node->Children[ChildIndex])
	{
		return RemoveFromNode(Node->Children[ChildIndex].Get(), Point);
	}
	return false;
}

bool FQuadtree::FindNearest(const FIntPoint& SearchPoint, float MaxSearchRadius, FIntPoint& OutNearestPoint) const
{
	float MinDistanceSq = MaxSearchRadius * MaxSearchRadius;
	FindNearestRecursive(Root.Get(), SearchPoint, MinDistanceSq, OutNearestPoint);
	return MinDistanceSq < MaxSearchRadius * MaxSearchRadius;
}

void FQuadtree::Subdivide(FQuadtreeNode* Node)
{
	const FVector2D Center = Node->Bounds.GetCenter();
	const FVector2D HalfSize = Node->Bounds.GetSize() / 2.0f;

	Node->Children[0] = MakeUnique<FQuadtreeNode>(FBox2D(Node->Bounds.Min, Center));
	Node->Children[1] = MakeUnique<FQuadtreeNode>(FBox2D(FVector2D(Center.X, Node->Bounds.Min.Y), FVector2D(Node->Bounds.Max.X, Center.Y)));
	Node->Children[2] = MakeUnique<FQuadtreeNode>(FBox2D(FVector2D(Node->Bounds.Min.X, Center.Y), FVector2D(Center.X, Node->Bounds.Max.Y)));
	Node->Children[3] = MakeUnique<FQuadtreeNode>(FBox2D(Center, Node->Bounds.Max));

	for (const FIntPoint& Point : Node->Points)
	{
		Node->Children[GetChildIndexForPoint(Node, Point)]->Points.Add(Point);
	}

	Node->Points.Empty();
}

int32 FQuadtree::GetChildIndexForPoint(const FQuadtreeNode* Node, const FIntPoint& Point) const
{
	const FVector2D Center = Node->Bounds.GetCenter();
	if (Point.X < Center.X)
	{
		return (Point.Y < Center.Y) ? 0 : 2;
	}
	else
	{
		return (Point.Y < Center.Y) ? 1 : 3;
	}
}

void FQuadtree::FindNearestRecursive(const FQuadtreeNode* Node, const FIntPoint& SearchPoint, float& MinDistanceSq, FIntPoint& OutNearestPoint) const
{
	// Calculate the squared distance from the search point to the closest point on the node's bounding box.
	float ClosestDistSqToNode = Node->Bounds.ComputeSquaredDistanceToPoint(FVector2D(SearchPoint.X, SearchPoint.Y));

	// If the closest point on this node's bounding box is further than the current best found point,
	// then this node (and its children) cannot contain a better point. Prune this branch.
	if (ClosestDistSqToNode >= MinDistanceSq)
	{
		return;
	}

	for (const FIntPoint& Point : Node->Points)
	{
		float DistSq = FVector::DistSquared(FVector(Point.X, Point.Y, 0), FVector(SearchPoint.X, SearchPoint.Y, 0));
		if (DistSq < MinDistanceSq)
		{
			MinDistanceSq = DistSq;
			OutNearestPoint = Point;
		}
	}

	if (Node->Children[0])
	{
		// Recursively search children, prioritizing closer children first
		TArray<TPair<float, FQuadtreeNode*>> SortedChildren;
		for (int32 i = 0; i < 4; ++i)
		{
			SortedChildren.Add(TPair<float, FQuadtreeNode*>(Node->Children[i]->Bounds.ComputeSquaredDistanceToPoint(FVector2D(SearchPoint.X, SearchPoint.Y)), Node->Children[i].Get()));
		}
		SortedChildren.Sort([](const TPair<float, FQuadtreeNode*>& A, const TPair<float, FQuadtreeNode*>& B) { return A.Key < B.Key; });

		for (const TPair<float, FQuadtreeNode*>& ChildPair : SortedChildren)
		{
			FindNearestRecursive(ChildPair.Value, SearchPoint, MinDistanceSq, OutNearestPoint);
		}
	}
}
