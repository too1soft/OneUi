use oneui::{Error, ImageContentMode, ImageView};

pub fn image() -> Result<ImageView, Error> {
    let image = ImageView::new()?;
    image.set_rgba(&[196, 237, 135, 255, 48, 98, 78, 255], 2, 1)?;
    image.set_content_mode(ImageContentMode::Contain);
    image.set_corner_radius(12.0);
    Ok(image)
}
