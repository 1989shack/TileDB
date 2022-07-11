/**
 * @file   result_coords.h
 *
 * @section LICENSE
 *
 * The MIT License
 *
 * @copyright Copyright (c) 2017-2022 TileDB, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * @section DESCRIPTION
 *
 * This file defines struct ResultCoords.
 */

#ifndef TILEDB_RESULT_COORDS_H
#define TILEDB_RESULT_COORDS_H

#include <iostream>
#include <vector>

#include "tiledb/common/types/dynamic_typed_datum.h"
#include "tiledb/sm/array_schema/dimension.h"
#include "tiledb/sm/query/readers/result_tile.h"

using namespace tiledb::common;

namespace tiledb {
namespace sm {

template <class BitmapType>
class GlobalOrderResultTile;

/**
 * Stores information about cell coordinates of a sparse fragment
 * that are in the result of a subarray query.
 */
template <class ResultTileType>
struct ResultCoordsBase {
  /**
   * The result tile the coords belong to.
   *
   * Note that the tile this points to is allocated and freed in
   * sparse_read/dense_read, so the lifetime of this struct must not exceed
   * the scope of those functions.
   */
  ResultTileType* tile_;
  /** The position of the coordinates in the tile. */
  uint64_t pos_;

  /** Constructor. */
  ResultCoordsBase()
      : tile_(nullptr)
      , pos_(0) {
  }

  /** Constructor. */
  ResultCoordsBase(ResultTileType* tile, uint64_t pos)
      : tile_(tile)
      , pos_(pos) {
  }

  /**
   * Returns a string coordinate. Applicable only to string
   * dimensions.
   */
  inline std::string_view coord_string(unsigned dim_idx) const {
    return tile_->coord_string(pos_, dim_idx);
  }

  /**
   * Returns the coordinate at the object's position `pos_` from the object's
   * tile `tile_` on the given dimension.
   *
   * @param dim_idx The index of the dimension to retrieve the coordinate for.
   * @return A constant pointer to the requested coordinate.
   */
  inline const void* coord(unsigned dim_idx) const {
    return tile_->coord(pos_, dim_idx);
  }

  inline UntypedDatumView dimension_datum(
      const Dimension& dim, unsigned dim_idx) const {
    if (dim.var_size()) {
      auto x{tile_->coord_string(pos_, dim_idx)};
      return tdb::UntypedDatumView{x.data(), x.size()};
    } else {
      return tdb::UntypedDatumView{coord(dim_idx), dim.coord_size()};
    }
  }

  /**
   * Returns true if the coordinates (at the current position) of the
   * calling ResultCoords object and the input are the same across all
   * dimensions.
   */
  bool same_coords(const ResultCoordsBase& rc) const {
    return tile_->same_coords(*(rc.tile_), pos_, rc.pos_);
  }
};

struct ResultCoords : public ResultCoordsBase<ResultTile> {
  /** Constructor. */
  ResultCoords()
      : ResultCoordsBase()
      , valid_(false) {
  }

  /** Constructor. */
  ResultCoords(ResultTile* tile, uint64_t pos)
      : ResultCoordsBase(tile, pos)
      , valid_(true) {
  }

  /** Invalidate this instance. */
  inline void invalidate() {
    valid_ = false;
  }

  /** Return true if this instance is valid. */
  inline bool valid() const {
    return valid_;
  }

  /** Whether this instance is "valid". */
  bool valid_;
};

template <class BitmapType>
struct GlobalOrderResultCoords
    : public ResultCoordsBase<GlobalOrderResultTile<BitmapType>> {
  using base = ResultCoordsBase<GlobalOrderResultTile<BitmapType>>;

  /** Constructor. */
  GlobalOrderResultCoords(GlobalOrderResultTile<BitmapType>* tile, uint64_t pos)
      : ResultCoordsBase<GlobalOrderResultTile<BitmapType>>(tile, pos)
      , init_(false) {
  }

  /** Advance to the next available cell in the tile. */
  bool advance_to_next_cell() {
    base::pos_ += init_;
    init_ = true;
    uint64_t cell_num = base::tile_->cell_num();
    if (base::pos_ != cell_num) {
      if (base::tile_->has_bmp()) {
        while (base::pos_ < cell_num) {
          if (base::tile_->bitmap_[base::pos_]) {
            return true;
          }
          base::pos_++;
        }
      } else {
        return true;
      }
    }

    return false;
  }

  /**
   * Get the maximum slab length that can be created (when there's no other
   * fragments left).
   */
  uint64_t max_slab_length() {
    uint64_t ret = 1;
    uint64_t cell_num = base::tile_->cell_num();
    uint64_t next_pos = base::pos_ + 1;
    if (base::tile_->has_post_qc_bmp()) {
      auto& bitmap = base::tile_->bitmap_with_qc();

      // Current cell is not in the bitmap.
      if (!bitmap[base::pos_]) {
        return 0;
      }

      // For overlapping ranges, if there's more than one cell in the bitmap,
      // return 1.
      const bool overlapping_ranges = std::is_same<BitmapType, uint64_t>::value;
      if constexpr (overlapping_ranges) {
        if (bitmap[base::pos_] != 1) {
          return 1;
        }
      }

      // With bitmap, find the longest contiguous set of bits in the bitmap
      // from the current position.
      while (next_pos < cell_num && bitmap[next_pos] == 1) {
        next_pos++;
        ret++;
      }
    } else {
      // No bitmap, add all cells from current position.
      ret = cell_num - base::pos_;
    }

    return ret;
  }

  /**
   * Get the maximum slab length that can be created using the next result
   * coords in the queue.
   */
  template <class CompType>
  uint64_t max_slab_length(
      const GlobalOrderResultCoords& next, const CompType& cmp) {
    uint64_t ret = 1;
    uint64_t cell_num = base::tile_->cell_num();

    // Store the original position.
    uint64_t orig_pos = base::pos_;

    if (base::tile_->has_post_qc_bmp()) {
      auto& bitmap = base::tile_->bitmap_with_qc();

      // Current cell is not in the bitmap.
      if (!bitmap[base::pos_]) {
        return 0;
      }

      // For overlapping ranges, if there's more than one cell in the bitmap,
      // return 1.
      const bool overlapping_ranges = std::is_same<BitmapType, uint64_t>::value;
      if constexpr (overlapping_ranges) {
        if (bitmap[base::pos_] != 1) {
          return 1;
        }
      }

      // With bitmap, find the longest contiguous set of bits in the bitmap
      // from the current position, with coordinares smaller than the next one
      // in the queue.
      base::pos_++;
      while (base::pos_ < cell_num && bitmap[base::pos_] && !cmp(*this, next)) {
        base::pos_++;
        ret++;
      }
    } else {
      // No bitmap, add all cells from current position, with coordinares
      // smaller than the next one in the queue.
      base::pos_++;
      while (base::pos_ < cell_num - 1 && !cmp(*this, next)) {
        base::pos_++;
        ret++;
      }
    }

    // Restore the original position.
    base::pos_ = orig_pos;
    return ret;
  }

 private:
  /** Initially set to false on first call to next_cell. */
  bool init_;
};

}  // namespace sm
}  // namespace tiledb

#endif  // TILEDB_RESULT_COORDS_H