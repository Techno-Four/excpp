#include <graphics/image.h>

#include <cassert>

#include <graphics/png.h>
#include <graphics/graphics.hpp>

namespace graphics
{

VkFormat get_format( Png& png )
{
	if ( png.color_type == PNG_COLOR_TYPE_RGB )
	{
		if ( png.bit_depth == 8 && png.channels == 3 )
		{
			return VK_FORMAT_R8G8B8_UNORM;
		}
	}

	assert( false && "Vulkan format not supported" );

	return VK_FORMAT_UNDEFINED;
}

Image::Image( Device& d, Png& png )
: device { d }
, extent { png.width, png.height, 1 }
, format { get_format( png ) }
, command_pool { d }
{
	// Image
	VkImageCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.imageType = VK_IMAGE_TYPE_2D;
	info.extent = extent;
	info.mipLevels = 1;
	info.arrayLayers = 1;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.format = format;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.samples = VK_SAMPLE_COUNT_1_BIT;

	auto res = vkCreateImage( device.handle, &info, nullptr, &handle );
	assert( res == VK_SUCCESS && "Cannot create image" );

	// Memory
	{
		VkMemoryRequirements requirements;
		vkGetImageMemoryRequirements( device.handle, handle, &requirements);

		VkMemoryAllocateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		info.allocationSize = requirements.size;
		auto memory_type = device.physical_device.get_memory_type(
			requirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		info.memoryTypeIndex = memory_type;

		auto res = vkAllocateMemory( device.handle, &info, nullptr, &memory );
		assert( res == VK_SUCCESS && "Cannot allocate memory for image" );

		vkBindImageMemory( device.handle, handle, memory, 0 );
	}
}


Image::~Image()
{
	if ( handle != VK_NULL_HANDLE )
	{
		vkFreeMemory( device.handle, memory, nullptr );
		vkDestroyImage( device.handle, handle, nullptr );
	}
}


Image::Image( Image&& other )
: device { other.device }
, extent { other.extent }
, format { other.format }
, handle { other.handle }
, memory { other.memory }
, layout { other.layout }
, command_pool { std::move( other.command_pool ) }
{
	other.handle = VK_NULL_HANDLE;
	other.memory = VK_NULL_HANDLE;
}


Image& Image::operator=( Image&& other )
{
	assert( device.handle == other.device.handle && "Cannot move images from different device" );
	std::swap( extent, other.extent );
	std::swap( format, other.format );
	std::swap( handle, other.handle );
	std::swap( memory, other.memory );
	std::swap( layout, other.layout );
	std::swap( command_pool, other.command_pool );

	return *this;
}


void Image::upload( Buffer& buffer )
{
	auto cmds = command_pool.allocate_command_buffers();
	auto& cmd = cmds[0];

	cmd.begin( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );

	cmd.transition( *this, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );
	cmd.copy( buffer, *this );
	cmd.transition( *this, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );

	cmd.end();

	auto& queue = device.find_graphics_queue();
	auto fence = Fence( device );
	fence.reset();
	queue.submit( cmd, {}, {}, &fence );
}


ImageView::ImageView( Device& d , Image& i )
: device { d }
, image { i }
{
	VkImageViewCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.image = image.handle;
	info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	info.format = image.format;
	info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	info.subresourceRange.baseMipLevel = 0;
	info.subresourceRange.levelCount = 1;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.layerCount = 1;

	auto res = vkCreateImageView( device.handle, &info, nullptr, &handle );
	assert( res == VK_SUCCESS && "Cannot create image view" );
}


ImageView::~ImageView()
{
	if ( handle != VK_NULL_HANDLE )
	{
		vkDestroyImageView( device.handle, handle, nullptr );
	}
}


ImageView::ImageView( ImageView&& other )
: device { other.device }
, image { other.image }
, handle { other.handle }
{
	other.handle = VK_NULL_HANDLE;
}


ImageView& ImageView::operator=( ImageView&& other )
{
	assert( device.handle == other.device.handle && "Cannot move images from different device" );
	assert( image.handle == other.image.handle && "Cannot move image views from different images" );
	std::swap( handle, other.handle );

	return *this;
}


} // namespace graphics
