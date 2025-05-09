#include "TilesetHeightQuery.h"

#include "TilesetContentManager.h"

#include <Cesium3DTilesSelection/BoundingVolume.h>
#include <Cesium3DTilesSelection/ITilesetHeightSampler.h>
#include <Cesium3DTilesSelection/SampleHeightResult.h>
#include <Cesium3DTilesSelection/Tile.h>
#include <Cesium3DTilesSelection/TileContent.h>
#include <Cesium3DTilesSelection/TileRefine.h>
#include <CesiumAsync/Promise.h>
#include <CesiumGeometry/BoundingCylinderRegion.h>
#include <CesiumGeometry/IntersectionTests.h>
#include <CesiumGeospatial/BoundingRegion.h>
#include <CesiumGeospatial/BoundingRegionWithLooseFittingHeights.h>
#include <CesiumGeospatial/Cartographic.h>
#include <CesiumGeospatial/Ellipsoid.h>
#include <CesiumGeospatial/GlobeRectangle.h>
#include <CesiumGeospatial/S2CellBoundingVolume.h>
#include <CesiumGltfContent/GltfUtilities.h>

#include <glm/exponential.hpp>

#include <cstddef>
#include <iterator>
#include <list>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using namespace Cesium3DTilesSelection;
using namespace CesiumGeospatial;
using namespace CesiumGeometry;
using namespace CesiumUtility;
using namespace CesiumAsync;

namespace {
bool boundingVolumeContainsCoordinate(
    const BoundingVolume& boundingVolume,
    const Ray& ray,
    const Cartographic& coordinate,
    const Ellipsoid& ellipsoid) {
  struct Operation {
    const Ray& ray;
    const Cartographic& coordinate;
    const Ellipsoid& ellipsoid;

    bool operator()(const OrientedBoundingBox& boundingBox) noexcept {
      std::optional<double> t =
          IntersectionTests::rayOBBParametric(ray, boundingBox);
      return t && t.value() >= 0;
    }

    bool operator()(const BoundingRegion& boundingRegion) noexcept {
      return boundingRegion.getRectangle().contains(coordinate);
    }

    bool operator()(const BoundingSphere& boundingSphere) noexcept {
      std::optional<double> t =
          IntersectionTests::raySphereParametric(ray, boundingSphere);
      return t && t.value() >= 0;
    }

    bool operator()(
        const BoundingRegionWithLooseFittingHeights& boundingRegion) noexcept {
      return boundingRegion.getBoundingRegion().getRectangle().contains(
          coordinate);
    }

    bool operator()(const S2CellBoundingVolume& s2Cell) noexcept {
      return s2Cell.computeBoundingRegion(ellipsoid).getRectangle().contains(
          coordinate);
    }

    bool operator()(const BoundingCylinderRegion& cylinderRegion) noexcept {
      std::optional<double> t = IntersectionTests::rayOBBParametric(
          ray,
          cylinderRegion.toOrientedBoundingBox());
      return t && t.value() >= 0;
    }
  };

  return std::visit(Operation{ray, coordinate, ellipsoid}, boundingVolume);
}

// The ray for height queries starts at this fraction of the ellipsoid max
// radius above the ellipsoid surface. If a tileset surface is more than this
// distance above the ellipsoid, it may be missed by height queries.
// 0.007 is chosen to accomodate Olympus Mons, the tallest peak on Mars. 0.007
// is seven-tenths of a percent, or about 44,647 meters for WGS84, well above
// the highest point on Earth.
const double rayOriginHeightFraction = 0.007;

Ray createRay(const Cartographic& position, const Ellipsoid& ellipsoid) {
  Cartographic startPosition(
      position.longitude,
      position.latitude,
      ellipsoid.getMaximumRadius() * rayOriginHeightFraction);

  return Ray(
      Ellipsoid::WGS84.cartographicToCartesian(startPosition),
      -Ellipsoid::WGS84.geodeticSurfaceNormal(startPosition));
}

} // namespace

TilesetHeightQuery::TilesetHeightQuery(
    const Cartographic& position,
    const Ellipsoid& ellipsoid_)
    : inputPosition(position),
      ray(createRay(position, ellipsoid_)),
      ellipsoid(ellipsoid_),
      intersection(),
      additiveCandidateTiles(),
      candidateTiles(),
      previousCandidateTiles() {}

Cesium3DTilesSelection::TilesetHeightQuery::~TilesetHeightQuery() = default;

void TilesetHeightQuery::intersectVisibleTile(
    Tile* pTile,
    std::vector<std::string>& outWarnings) {
  TileRenderContent* pRenderContent = pTile->getContent().getRenderContent();
  if (!pRenderContent)
    return;

  auto gltfIntersectResult =
      CesiumGltfContent::GltfUtilities::intersectRayGltfModel(
          this->ray,
          pRenderContent->getModel(),
          true,
          pTile->getTransform());

  if (!gltfIntersectResult.warnings.empty()) {
    outWarnings.insert(
        outWarnings.end(),
        std::make_move_iterator(gltfIntersectResult.warnings.begin()),
        std::make_move_iterator(gltfIntersectResult.warnings.end()));
  }

  // Set ray info to this hit if closer, or the first hit
  if (!this->intersection.has_value()) {
    this->intersection = std::move(gltfIntersectResult.hit);
  } else if (gltfIntersectResult.hit) {
    double prevDistSq = this->intersection->rayToWorldPointDistanceSq;
    double thisDistSq = gltfIntersectResult.hit->rayToWorldPointDistanceSq;
    if (thisDistSq < prevDistSq)
      this->intersection = std::move(gltfIntersectResult.hit);
  }
}

void TilesetHeightQuery::findCandidateTiles(
    Tile* pTile,
    std::vector<std::string>& warnings) {
  // If tile failed to load, this means we can't complete the intersection
  if (pTile->getState() == TileLoadState::Failed) {
    warnings.emplace_back("Tile load failed during query. Ignoring.");
    return;
  }

  const std::optional<BoundingVolume>& contentBoundingVolume =
      pTile->getContentBoundingVolume();

  if (pTile->getChildren().empty()) {
    // This is a leaf node, it's a candidate

    // If optional content bounding volume exists, test against it
    if (contentBoundingVolume) {
      if (boundingVolumeContainsCoordinate(
              *contentBoundingVolume,
              this->ray,
              this->inputPosition,
              this->ellipsoid)) {
        this->candidateTiles.emplace_back(pTile);
      }
    } else {
      this->candidateTiles.emplace_back(pTile);
    }
  } else {
    // We have children

    // If additive refinement, add parent to the list with children
    if (pTile->getRefine() == TileRefine::Add) {
      // If optional content bounding volume exists, test against it
      if (contentBoundingVolume) {
        if (boundingVolumeContainsCoordinate(
                *contentBoundingVolume,
                this->ray,
                this->inputPosition,
                this->ellipsoid)) {
          this->additiveCandidateTiles.emplace_back(pTile);
        }
      } else {
        this->additiveCandidateTiles.emplace_back(pTile);
      }
    }

    // Traverse children
    for (Tile& child : pTile->getChildren()) {
      // if bounding volume doesn't intersect this ray, we can skip it
      if (!boundingVolumeContainsCoordinate(
              child.getBoundingVolume(),
              this->ray,
              this->inputPosition,
              this->ellipsoid))
        continue;

      // Child is a candidate, traverse it and its children
      findCandidateTiles(&child, warnings);
    }
  }
}

TilesetHeightRequest::TilesetHeightRequest(
    std::vector<TilesetHeightQuery>&& queries_,
    const CesiumAsync::Promise<SampleHeightResult>& promise_) noexcept
    : queries(std::move(queries_)), promise(promise_) {}

TilesetHeightRequest::TilesetHeightRequest(
    TilesetHeightRequest&& rhs) noexcept = default;

TilesetHeightRequest::~TilesetHeightRequest() noexcept = default;

/*static*/ void TilesetHeightRequest::processHeightRequests(
    const AsyncSystem& asyncSystem,
    TilesetContentManager& contentManager,
    const TilesetOptions& options,
    std::list<TilesetHeightRequest>& heightRequests) {
  if (heightRequests.empty())
    return;

  // Go through all requests, either complete them, or gather the tiles they
  // need for completion
  for (auto it = heightRequests.begin(); it != heightRequests.end();) {
    TilesetHeightRequest& request = *it;
    if (!request
             .tryCompleteHeightRequest(asyncSystem, contentManager, options)) {
      ++it;
    } else {
      auto deleteIt = it;
      ++it;
      heightRequests.erase(deleteIt);
    }
  }
}

double TilesetHeightRequest::getWeight() const { return 1.0; }

bool TilesetHeightRequest::hasMoreTilesToLoadInWorkerThread() const {
  return !this->tilesToLoad.empty();
}

Tile* TilesetHeightRequest::getNextTileToLoadInWorkerThread() {
  Tile* pResult = nullptr;

  auto it = this->tilesToLoad.begin();
  if (it != this->tilesToLoad.end()) {
    pResult = *it;
    this->tilesToLoad.erase(it);
  }

  return pResult;
}

bool TilesetHeightRequest::hasMoreTilesToLoadInMainThread() const {
  // We don't need to do any main thread loading for height queries.
  return false;
}

Tile* TilesetHeightRequest::getNextTileToLoadInMainThread() { return nullptr; }

void Cesium3DTilesSelection::TilesetHeightRequest::failHeightRequests(
    std::list<TilesetHeightRequest>& heightRequests,
    const std::string& message) {
  for (TilesetHeightRequest& request : heightRequests) {
    SampleHeightResult result;
    result.warnings.emplace_back(message);
    result.sampleSuccess.resize(request.queries.size(), false);

    result.positions.reserve(request.queries.size());
    for (const TilesetHeightQuery& query : request.queries) {
      result.positions.emplace_back(query.inputPosition);
    }

    request.promise.resolve(std::move(result));
  }

  heightRequests.clear();
}

bool TilesetHeightRequest::tryCompleteHeightRequest(
    const AsyncSystem& asyncSystem,
    TilesetContentManager& contentManager,
    const TilesetOptions& options) {
  this->tilesToLoad.clear();

  // If this TilesetContentLoader supports direct height queries, use that
  // instead of downloading tiles.
  if (contentManager.getRootTile() &&
      contentManager.getRootTile()->getLoader()) {
    ITilesetHeightSampler* pSampler =
        contentManager.getRootTile()->getLoader()->getHeightSampler();
    if (pSampler) {
      std::vector<Cartographic> positions;
      positions.reserve(this->queries.size());
      for (TilesetHeightQuery& query : this->queries) {
        positions.emplace_back(query.inputPosition);
      }

      pSampler->sampleHeights(asyncSystem, std::move(positions))
          .thenImmediately(
              [promise = this->promise](SampleHeightResult&& result) {
                promise.resolve(std::move(result));
              });

      return true;
    }
  }

  // No direct height query possible, so download and sample tiles.
  bool tileStillNeedsLoading = false;
  std::vector<std::string> warnings;
  for (TilesetHeightQuery& query : this->queries) {
    if (query.candidateTiles.empty() && query.additiveCandidateTiles.empty()) {
      // Find the initial set of tiles whose bounding volume is intersected by
      // the query ray.
      query.findCandidateTiles(contentManager.getRootTile(), warnings);
    } else {
      // Refine the current set of candidate tiles, in case further tiles from
      // implicit tiling, external tilesets, etc. having been loaded since last
      // frame.
      std::swap(query.candidateTiles, query.previousCandidateTiles);

      query.candidateTiles.clear();

      for (const Tile::Pointer& pCandidate : query.previousCandidateTiles) {
        TileLoadState loadState = pCandidate->getState();
        if (!pCandidate->getChildren().empty() &&
            loadState >= TileLoadState::ContentLoaded) {
          query.findCandidateTiles(pCandidate.get(), warnings);
        } else {
          // Check again next frame to see if this tile has children.
          query.candidateTiles.emplace_back(pCandidate);
        }
      }
    }

    auto checkTile =
        [this, &contentManager, &options, &tileStillNeedsLoading](Tile* pTile) {
          contentManager.createLatentChildrenIfNecessary(*pTile, options);

          TileLoadState state = pTile->getState();
          if (state == TileLoadState::Unloading) {
            // This tile is in the process of unloading, which must complete
            // before we can load it again.
            contentManager.unloadTileContent(*pTile);
            tileStillNeedsLoading = true;
          } else if (state <= TileLoadState::ContentLoading) {
            this->tilesToLoad.insert(pTile);
            tileStillNeedsLoading = true;
          }
        };

    // If any candidates need loading, add to return set
    for (const Tile::Pointer& pTile : query.additiveCandidateTiles) {
      checkTile(pTile.get());
    }
    for (const Tile::Pointer& pTile : query.candidateTiles) {
      checkTile(pTile.get());
    }
  }

  // Bail if we're waiting on tiles to load
  if (tileStillNeedsLoading)
    return false;

  // Do the intersect tests
  for (TilesetHeightQuery& query : this->queries) {
    for (const Tile::Pointer& pTile : query.additiveCandidateTiles) {
      query.intersectVisibleTile(pTile.get(), warnings);
    }
    for (const Tile::Pointer& pTile : query.candidateTiles) {
      query.intersectVisibleTile(pTile.get(), warnings);
    }
  }

  // All rays are done, create results
  SampleHeightResult results;

  // Start with any warnings from tile traversal
  results.warnings = std::move(warnings);

  results.positions.resize(this->queries.size(), Cartographic(0.0, 0.0, 0.0));
  results.sampleSuccess.resize(this->queries.size());

  // Populate results with completed queries
  for (size_t i = 0; i < this->queries.size(); ++i) {
    const TilesetHeightQuery& query = this->queries[i];

    bool sampleSuccess = query.intersection.has_value();
    results.sampleSuccess[i] = sampleSuccess;
    results.positions[i] = query.inputPosition;

    if (sampleSuccess) {
      results.positions[i].height =
          options.ellipsoid.getMaximumRadius() * rayOriginHeightFraction -
          glm::sqrt(query.intersection->rayToWorldPointDistanceSq);
    }
  }

  this->promise.resolve(std::move(results));
  return true;
}
