import { plain, withHighlights } from "../lib/text";
import { BackendReadImage } from "../lib/types";

export function RenderedText({
  text,
  highlights,
  images,
}: {
  text: string;
  highlights: string[];
  images: BackendReadImage[];
}) {
  const imageByMarker = Object.fromEntries(images.map((image) => [image.marker, image]));
  return (
    <>
      {text.split("\n\n").map((block, index) => {
        const image = imageByMarker[block.trim()];
        if (image) {
          return (
            // eslint-disable-next-line @next/next/no-img-element
            <img
              key={`${block}-${index}`}
              className="inline-reader-image"
              src={`data:${image.mimeType};base64,${image.dataBase64}`}
              alt=""
            />
          );
        }
        if (block.startsWith("# ")) return <h1 key={index}>{plain(block.slice(2))}</h1>;
        if (block.startsWith("## ")) return <h2 key={index}>{plain(block.slice(3))}</h2>;
        if (block.startsWith("### ")) return <h3 key={index}>{plain(block.slice(4))}</h3>;
        return <p key={index}>{withHighlights(plain(block), highlights)}</p>;
      })}
    </>
  );
}
