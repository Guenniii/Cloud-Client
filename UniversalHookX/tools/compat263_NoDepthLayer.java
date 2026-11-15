package kaptainwutax.seedcrackerX.render;
import com.mojang.renderpearl.api.pipeline.DepthStencilState;
import com.mojang.renderpearl.api.pipeline.RenderPipeline;
import com.mojang.renderpearl.api.pipeline.CompareOp;
import net.minecraft.client.renderer.RenderPipelines;
import net.minecraft.client.renderer.rendertype.LayeringTransform;
import net.minecraft.client.renderer.rendertype.RenderSetup;
import net.minecraft.client.renderer.rendertype.RenderType;
import net.minecraft.resources.Identifier;
public final class NoDepthLayer {
 private NoDepthLayer() {}
 private static RenderType create(){
  try {
   var snippet=RenderPipelines.class.getDeclaredField("LINES_SNIPPET");snippet.setAccessible(true);
   var pipeline=RenderPipeline.builder((RenderPipeline.Snippet)snippet.get(null))
    .withLocation(Identifier.fromNamespaceAndPath("seedcrackerx","pipeline/lines_no_depth"))
    .withDepthStencilState(new DepthStencilState(CompareOp.ALWAYS_PASS,false,0,0)).build();
   var register=RenderPipelines.class.getDeclaredMethod("register",RenderPipeline.class);register.setAccessible(true);register.invoke(null,pipeline);
   var setup=RenderSetup.builder(pipeline).setLayeringTransform(LayeringTransform.VIEW_OFFSET_Z_LAYERING).createRenderSetup();
   var create=RenderType.class.getDeclaredMethod("create",String.class,RenderSetup.class);create.setAccessible(true);
   return (RenderType)create.invoke(null,"seedcrackerx_no_depth",setup);
  }catch(ReflectiveOperationException error){throw new IllegalStateException("26.3 no-depth renderer unavailable",error);}
 }
 public static final RenderType LINES_NO_DEPTH_LAYER=create();
}
