package com.vtec.ediwand

import android.graphics.Rect
import android.view.View
import androidx.core.view.children
import androidx.core.view.marginTop
import androidx.recyclerview.widget.RecyclerView

class ItemDecoration(private val columns: Int): RecyclerView.ItemDecoration()
{
    override fun getItemOffsets(
        outRect: Rect,
        view: View,
        parent: RecyclerView,
        state: RecyclerView.State
    ) {
        val position = parent.getChildAdapterPosition(view)

        val margin = parent.resources.getDimension(R.dimen.screen_bottom_margin).toInt()

        parent.adapter?.let {
            if ((position + 1) % columns == 0) outRect.right = parent.children.first().marginTop
            if (columns == 1)
                if (position >= it.itemCount - columns) outRect.bottom = margin
        }
    }
}
